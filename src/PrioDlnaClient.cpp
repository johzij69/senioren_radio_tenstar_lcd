#include "PrioDlnaClient.h"
#include <esp32-hal-log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdlib.h>
#include <LittleFS.h>

// Cache of the last successful seekServer() result, so a normal (non-forced)
// seek can skip the ~8s SSDP M-SEARCH wait entirely on subsequent opens of the
// DLNA browser. Home-network media servers rarely change IP, so this is safe
// to reuse until the user explicitly asks for a rescan (or a cached server
// turns out to be unreachable when actually browsed).
static const char* DLNA_SERVER_CACHE_FILE = "/dlna_servers.cache";

// Discovery/browse protocol logic adapted from
// https://github.com/schreibfaul1/ESP32-DLNA-Client (SSDP M-SEARCH discovery +
// UPnP ContentDirectory SOAP Browse), wired up to instance callbacks instead of
// weak global functions so it fits this project's Prio* module conventions.

PrioDlnaClient::PrioDlnaClient() {
    m_state = IDLE;
    m_chunked = false;
    m_PSRAMfound = psramInit();
    m_chbuf = (char*)malloc(512);
    m_chbufSize = 512;
}

PrioDlnaClient::~PrioDlnaClient() {
    dlnaServer_clear_and_shrink();
    srvContent_clear_and_shrink();
    vector_clear_and_shrink(m_content);
    if (m_chbuf) { free(m_chbuf); m_chbuf = nullptr; }
    if (m_JSONstr) { free(m_JSONstr); m_JSONstr = nullptr; }
}
//------------------------------------------------------------------------------------------------
void PrioDlnaClient::setCallbacks(InfoCallback info, ServerFoundCallback serverFound,
                                   SeekReadyCallback seekReady, BrowseResultCallback browseResult,
                                   BrowseReadyCallback browseReady) {
    _infoCallback = info;
    _serverFoundCallback = serverFound;
    _seekReadyCallback = seekReady;
    _browseResultCallback = browseResult;
    _browseReadyCallback = browseReady;
}
//------------------------------------------------------------------------------------------------
bool PrioDlnaClient::seekServer(bool forceRescan) {
    if (!forceRescan && loadServerCache()) {
        m_state = IDLE;
        if (_infoCallback) _infoCallback("DLNA-servers geladen uit cache");
        if (_seekReadyCallback) _seekReadyCallback(m_dlnaServer.size);
        return true;
    }

    if (WiFi.status() != WL_CONNECTED) return false; // guard

    if (m_chbuf) { free(m_chbuf); m_chbuf = nullptr; }
    if (!m_PSRAMfound) {
        m_chbuf = (char*)malloc(512);
        m_chbufSize = 512;
    } else {
        m_chbuf = (char*)ps_malloc(4 * 4096);
        m_chbufSize = 4 * 4096;
    }

    dlnaServer_clear_and_shrink();
    m_dlnaServer.size = 0;
    uint8_t ret = 0;
    const char searchTX[] = "M-SEARCH * HTTP/1.1\r\n"
                             "HOST: 239.255.255.250:1900\r\n"
                             "MAN: \"ssdp:discover\"\r\n"
                             "MX: 3\r\n"
                             "ST: urn:schemas-upnp-org:device:MediaServer:1\r\n\r\n";

    ret = m_udp.beginMulticast(IPAddress(PRIO_DLNA_MULTICAST_IP), PRIO_DLNA_LOCAL_PORT);
    if (!ret) {
        m_udp.stop();
        log_e("error sending SSDP multicast packets");
        return false;
    }
    for (int i = 0; i < 3; i++) {
        ret = m_udp.beginPacket(IPAddress(PRIO_DLNA_MULTICAST_IP), PRIO_DLNA_MULTICAST_PORT);
        if (!ret) { log_e("udp beginPacket error"); return false; }
        ret = m_udp.write((const uint8_t*)searchTX, strlen(searchTX));
        if (!ret) { log_e("udp write error"); return false; }
        ret = m_udp.endPacket();
        if (!ret) { log_e("endPacket error"); return false; }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    m_state = SEEK_SERVER;
    m_timeStamp = millis();
    return true;
}
//------------------------------------------------------------------------------------------------
int8_t PrioDlnaClient::listServer() {
    if (m_state == SEEK_SERVER) return -1; // seek in progress
    for (uint8_t i = 0; i < m_dlnaServer.size; i++) {
        if (_serverFoundCallback) {
            _serverFoundCallback(i, m_dlnaServer.ip[i], m_dlnaServer.port[i],
                                  m_dlnaServer.friendlyName[i], m_dlnaServer.controlURL[i]);
        }
    }
    return m_dlnaServer.size;
}

PrioDlnaClient::dlnaServer_t PrioDlnaClient::getServer() {
    return m_dlnaServer;
}

PrioDlnaClient::srvContent_t PrioDlnaClient::getBrowseResult() {
    return m_srvContent;
}
//------------------------------------------------------------------------------------------------
void PrioDlnaClient::parseDlnaServer(uint16_t len) {
    if (len > m_chbufSize - 1) len = m_chbufSize - 1; // guard
    char* dummy = strdup("?");
    memset(m_chbuf, 0, m_chbufSize);
    vTaskDelay(200);
    m_udp.read(m_chbuf, len); // read packet into the buffer
    char* p = strcasestr(m_chbuf, "Location: http");
    if (!p) return;
    int idx1 = indexOf(p, "://", 0) + 3; // pos IP
    int idx2 = indexOf(p, ":", idx1);    // pos ':'
    int idx3 = indexOf(p, "/", idx2);    // pos '/'
    int idx4 = indexOf(p, "\r", idx3);   // pos '\r'
    *(p + idx2) = '\0';
    *(p + idx3) = '\0';
    *(p + idx4) = '\0';
    for (int i = 0; i < m_dlnaServer.size; i++) {
        if (strcmp(m_dlnaServer.ip[i], p + idx1) == 0) {      // same IP
            if (m_dlnaServer.port[i] == atoi(p + idx2 + 1)) { // same port
                free(dummy);
                return;
            }
        }
    }
    if (strcmp(p + idx1, "0.0.0.0") == 0) { log_e("invalid IP address found %s", p + idx1); free(dummy); return; }
    m_dlnaServer.ip.push_back(x_ps_strdup(p + idx1));
    m_dlnaServer.port.push_back(atoi(p + idx2 + 1));
    m_dlnaServer.location.push_back(x_ps_strdup(p + idx3 + 1));
    m_dlnaServer.controlURL.push_back(dummy);
    m_dlnaServer.friendlyName.push_back(strdup("?"));
    m_dlnaServer.presentationPort.push_back(0);
    m_dlnaServer.presentationURL.push_back(strdup("?"));
    m_dlnaServer.size++;
}
//------------------------------------------------------------------------------------------------
bool PrioDlnaClient::srvGet(uint8_t srvNr) {
    bool ret = false;
    m_client.stop();
    m_client.setTimeout(PRIO_DLNA_CONNECT_TIMEOUT);
    uint32_t t = millis();
    ret = m_client.connect(m_dlnaServer.ip[srvNr], m_dlnaServer.port[srvNr]);
    if (!ret) {
        m_client.stop();
        sprintf(m_chbuf, "The server %s:%d did not answer within %lums", m_dlnaServer.ip[srvNr], m_dlnaServer.port[srvNr], millis() - t);
        if (_infoCallback) _infoCallback(m_chbuf);
        return false;
    }
    t = millis() + 250;
    while (true) {
        if (m_client.connected()) break;
        if (t < millis()) {
            sprintf(m_chbuf, "The server %s:%d refuses the connection", m_dlnaServer.ip[srvNr], m_dlnaServer.port[srvNr]);
            if (_infoCallback) _infoCallback(m_chbuf);
            return false;
        }
    }
    // assemble HTTP header
    sprintf(m_chbuf, "GET /%s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\nUser-Agent: ESP32/Player/UPNP1.0\r\n\r\n",
            m_dlnaServer.location[srvNr], m_dlnaServer.ip[srvNr], m_dlnaServer.port[srvNr]);
    m_client.print(m_chbuf);
    t = millis() + PRIO_DLNA_AVAIL_TIMEOUT;
    while (true) {
        if (m_client.available()) break;
        if (t < millis()) {
            sprintf(m_chbuf, "The server %s:%d is not responding after request", m_dlnaServer.ip[srvNr], m_dlnaServer.port[srvNr]);
            if (_infoCallback) _infoCallback(m_chbuf);
            return false;
        }
    }
    return true;
}
//------------------------------------------------------------------------------------------------
bool PrioDlnaClient::readHttpHeader() {
    bool ct_seen = false;
    bool firstLine = true;
    m_timeStamp = millis();
    uint16_t rhlSize = 1024;
    char* rhl = x_ps_malloc(rhlSize); // response header line
    if (!rhl) return false;
    while (true) { // outer while
        uint16_t pos = 0;
        if ((m_timeStamp + PRIO_DLNA_READ_TIMEOUT) < millis()) {
            sprintf(m_chbuf, "timeout in readHttpHeader");
            if (_infoCallback) _infoCallback(m_chbuf);
            free(rhl);
            return false;
        }
        while (m_client.available()) {
            uint8_t b = m_client.read();
            if (b == '\n') {
                if (!pos) goto exit; // empty line received, last line of this responseHeader
                break;
            }
            if (b == '\r') rhl[pos] = 0;
            if (b < 0x20) continue;
            rhl[pos] = b;
            pos++;
            // On a fast local network available() can stay true for a long stretch;
            // yield periodically so this doesn't starve the idle task and trip the
            // 5s task watchdog (CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPUx in sdkconfig).
            if ((pos & 0x3F) == 0) vTaskDelay(1);
            if (pos == rhlSize - 1) { pos--; continue; }
            if (pos == rhlSize - 2) {
                rhl[pos] = '\0';
                sprintf(m_chbuf, "responseHeaderline overflow, response was: %s", rhl);
                if (_infoCallback) _infoCallback(m_chbuf);
            }
        } // inner while

        if (firstLine) {
            firstLine = false;
            sprintf(m_chbuf, "HTTP status: %s", rhl);
            if (_infoCallback) _infoCallback(m_chbuf);
            int16_t sp1 = indexOf(rhl, " ", 0);
            if (sp1 < 0 || rhl[sp1 + 1] != '2') { // anything other than a 2xx status is a failure
                free(rhl);
                return false;
            }
            continue; // status line has no ':' worth lower-casing/parsing below
        }

        {
            int16_t posColon = indexOf(rhl, ":", 0); // lowercase all letters up to the colon
            if (posColon >= 0) {
                for (int i = 0; i < posColon; i++) { rhl[i] = toLowerAscii(rhl[i]); }
            }
            if (startsWith(rhl, "content-length:")) {
                m_contentlength = atoi(rhl + 15);
            } else if (startsWith(rhl, "content-type:")) { // content-type: text/html; charset=UTF-8
                int idx = indexOf(rhl + 13, ";", 0);
                if (idx > 0) rhl[13 + idx] = '\0';
                if (indexOf(rhl + 13, "text/xml", 0) > 0) ct_seen = true;
                else if (indexOf(rhl + 13, "text/html", 0) > 0) ct_seen = true;
                else {
                    sprintf(m_chbuf, "content type expected: text/xml or text/html, got %s", rhl + 13);
                    if (_infoCallback) _infoCallback(m_chbuf);
                    goto exit; // wrong content type
                }
            } else if (startsWith(rhl, "transfer-encoding:")) {
                if (endsWith(rhl, "chunked") || endsWith(rhl, "Chunked")) m_chunked = true;
            }
        }
    } // outer while

exit:
    free(rhl);
    if (!m_contentlength) log_e("contentlength is not given");
    if (!ct_seen) log_e("content type not found");
    return true;
}
//------------------------------------------------------------------------------------------------
bool PrioDlnaClient::readContent() {
    m_timeStamp = millis();
    uint32_t idx = 0;
    uint8_t lastChar = 0;
    uint8_t b = 0;
    bool f_overflow = false;
    vector_clear_and_shrink(m_content);

    while (true) { // outer while
        uint16_t pos = 0;
        uint8_t cnt = 0;
        if ((m_timeStamp + PRIO_DLNA_READ_TIMEOUT) < millis()) {
            sprintf(m_chbuf, "timeout in readContent");
            if (_infoCallback) _infoCallback(m_chbuf);
            return false;
        }
        uint32_t idxBefore = idx;
        while (m_client.available()) {
            if (lastChar) { b = lastChar; lastChar = 0; }
            else { b = m_client.read(); idx++; }
            if (b == '\n') { m_chbuf[pos] = '\0'; break; }
            if (b == '<' && m_chbuf[pos - 1] == '>') { lastChar = '<'; m_chbuf[pos] = '\0'; break; } // simulate new line
            if (b == ';') { m_chbuf[pos] = '\0'; break; } // simulate new line
            if (b == '\r') m_chbuf[pos] = '\0';
            if (b < 0x20) continue;
            m_chbuf[pos] = b;
            pos++;
            // See readHttpHeader(): avoid starving the idle task / task watchdog
            // when the server streams data continuously on a fast local network.
            if ((idx & 0x3F) == 0) vTaskDelay(1);
            if (pos >= m_chbufSize - 1) { m_chbuf[pos] = '\0'; f_overflow = true; pos--; continue; }
            while (!m_client.available()) {
                vTaskDelay(10);
                cnt++;
                if (cnt == 100) { sprintf(m_chbuf, "timeout in readContent"); break; }
            }
        }
        if (f_overflow) log_e("line overflow");

        m_content.push_back(x_ps_strdup(m_chbuf));
        if (!m_chunked && idx == m_contentlength) break;
        if (!m_client.available()) {
            vTaskDelay(10);
            if (m_chunked) break; // ok
        }
        // Only push the deadline out when bytes actually arrived this iteration -
        // resetting it unconditionally here (as the upstream code does) means the
        // timeout can never fire while the loop keeps spinning, so a Content-Length
        // that doesn't quite match what the server actually sends causes an
        // effectively infinite hang instead of a clean failure.
        if (idx != idxBefore) {
            m_timeStamp = millis();
        }
    }
    return true;
}
//------------------------------------------------------------------------------------------------
bool PrioDlnaClient::getServerItems(uint8_t srvNr) {
    if (m_dlnaServer.size == 0) return false; // return if none detected

    bool gotFriendlyName = false;
    bool gotServiceType = false;
    bool URNschemaFound = false;

    for (size_t i = 0; i < m_content.size(); i++) {
        uint16_t idx = 0;
        while (*(m_content[i] + idx) == 0x20) idx++; // same as trim left
        char* content = m_content[i] + idx;
        if (!gotFriendlyName) {
            if (startsWith(content, "<friendlyName>")) {
                uint16_t pos = indexOf(content, "<", 14);
                *(content + pos) = '\0';
                if (m_dlnaServer.friendlyName[srvNr]) free(m_dlnaServer.friendlyName[srvNr]);
                if (strlen(content + 14) == 0) m_dlnaServer.friendlyName[srvNr] = strdup("Server name not provided");
                else m_dlnaServer.friendlyName[srvNr] = x_ps_strdup(content + 14);
                gotFriendlyName = true;
            }
        }
        if (!gotServiceType) {
            if (indexOf(content, "urn:schemas-upnp-org:service:ContentDirectory:1", 0) > 0) { URNschemaFound = true; continue; }
            if (URNschemaFound) {
                if (startsWith(content, "<controlURL>")) {
                    uint16_t pos = indexOf(content, "<", 12);
                    *(content + pos) = '\0';
                    if (m_dlnaServer.controlURL[srvNr]) free(m_dlnaServer.controlURL[srvNr]);
                    m_dlnaServer.controlURL[srvNr] = x_ps_strdup(content + 13);
                    gotServiceType = true;
                }
            }
        }
        if (startsWith(content, "<presentationURL>")) {
            uint16_t pos = indexOf(content, "<", 17);
            *(content + pos) = '\0';
            char* presentationURL = x_ps_strdup(content + 17);
            if (!presentationURL || !startsWith(presentationURL, "http://")) { if (presentationURL) free(presentationURL); continue; }
            int8_t posColon = (indexOf(presentationURL, ":", 8));
            if (m_dlnaServer.presentationURL[srvNr]) free(m_dlnaServer.presentationURL[srvNr]);
            if (posColon > 0) { // we have ip and port
                presentationURL[posColon] = '\0';
                m_dlnaServer.presentationURL[srvNr] = x_ps_strdup(presentationURL + 7);
                m_dlnaServer.presentationPort[srvNr] = atoi(presentationURL + posColon + 1);
            } else {
                m_dlnaServer.presentationURL[srvNr] = x_ps_strdup(presentationURL + 7);
            }
            free(presentationURL);
        }
    }

    // we finally got all infos we need
    uint16_t idx = 0;
    if (m_dlnaServer.location[srvNr] && endsWith(m_dlnaServer.location[srvNr], "/")) {
        char* tmp = (char*)malloc(strlen(m_dlnaServer.location[srvNr]) + strlen(m_dlnaServer.controlURL[srvNr]) + 1);
        strcpy(tmp, m_dlnaServer.location[srvNr]); // location string becomes first part of controlURL
        strcat(tmp, m_dlnaServer.controlURL[srvNr]);
        free(m_dlnaServer.controlURL[srvNr]);
        m_dlnaServer.controlURL[srvNr] = x_ps_strdup(tmp);
        free(tmp);
    }
    if (m_dlnaServer.controlURL[srvNr] && startsWith(m_dlnaServer.controlURL[srvNr], "http://")) { // remove "http://ip:port/" from begin of string
        idx = indexOf(m_dlnaServer.controlURL[srvNr], "/", 7);
        memmove(m_dlnaServer.controlURL[srvNr], m_dlnaServer.controlURL[srvNr] + idx + 1, strlen(m_dlnaServer.controlURL[srvNr]) - idx);
    }
    if (strcmp(m_dlnaServer.friendlyName[srvNr], "?") == 0) { log_e("friendlyName not found for server [%i]", srvNr); return false; }
    if (strcmp(m_dlnaServer.controlURL[srvNr], "?") == 0) { log_e("controlURL not found for server [%i]", srvNr); return false; }
    if (_serverFoundCallback) {
        _serverFoundCallback(srvNr, m_dlnaServer.ip[srvNr], m_dlnaServer.port[srvNr],
                              m_dlnaServer.friendlyName[srvNr], m_dlnaServer.controlURL[srvNr]);
    }
    return true;
}
//------------------------------------------------------------------------------------------------
bool PrioDlnaClient::browseResult() {
    auto makeContentPushBack = [&]() { // lambda, inner function
        m_srvContent.childCount.push_back(0);
        m_srvContent.isAudio.push_back(0);
        m_srvContent.itemSize.push_back(0);
        m_srvContent.itemURL.push_back(strdup("?"));
        m_srvContent.duration.push_back(strdup("?"));
        m_srvContent.objectId.push_back(strdup("?"));
        m_srvContent.parentId.push_back(strdup("?"));
        m_srvContent.title.push_back(strdup("?"));
        m_srvContent.artist.push_back(strdup("?"));
        m_srvContent.album.push_back(strdup("?"));
        m_srvContent.albumArtURI.push_back(strdup("?"));
        m_srvContent.size++;
    };

    m_numberReturned = 0;
    m_totalMatches = 0;
    bool item1 = false;
    bool item2 = false;
    int a, b, c, d;
    srvContent_clear_and_shrink();
    for (size_t i = 0; i < m_content.size(); i++) {
        uint16_t idx = 0;
        while (*(m_content[i] + idx) == 0x20) idx++; // same as trim left
        char* content = m_content[i] + idx;

        /*------ C O N T A I N E R -------*/
        if (startsWith(content, "container id")) { item1 = true; memset(m_chbuf, 0, m_chbufSize); }
        if (item1) strcat(m_chbuf, content);
        if (startsWith(content, "/container")) {
            item1 = false;
            uint16_t cNr = m_srvContent.size;
            makeContentPushBack();
            replacestr(m_chbuf, "&quot", "\"");
            replacestr(m_chbuf, "&ampamp", "&");
            replacestr(m_chbuf, "&ampapos", "'");
            replacestr(m_chbuf, "&ampquot", "\"");

            a = indexOf(m_chbuf, "container id=", 0);
            if (a >= 0) {
                a += 14;
                b = indexOf(m_chbuf, "\"", a);
                free(m_srvContent.objectId[cNr]);
                m_srvContent.objectId[cNr] = x_ps_strndup(m_chbuf + a, b - a);
            }
            a = indexOf(m_chbuf, "parentID=", 0);
            if (a >= 0) {
                a += 10;
                b = indexOf(m_chbuf, "\"", a);
                free(m_srvContent.parentId[cNr]);
                m_srvContent.parentId[cNr] = x_ps_strndup(m_chbuf + a, b - a);
            }
            a = indexOf(m_chbuf, "childCount=", 0);
            if (a >= 0) {
                a += 12;
                b = indexOf(m_chbuf, "\"", a);
                char tmp[10] = {0}; memcpy(tmp, m_chbuf + a, b - a);
                m_srvContent.childCount[cNr] = atoi(tmp);
            }
            a = indexOf(m_chbuf, "dc:title", 0);
            if (a >= 0) {
                a += 11;
                b = indexOf(m_chbuf, "/dc:title", a);
                b -= 3;
                free(m_srvContent.title[cNr]);
                m_srvContent.title[cNr] = x_ps_strndup(m_chbuf + a, b - a);
                if (strlen(m_srvContent.title[cNr]) == 0) { free(m_srvContent.title[cNr]); m_srvContent.title[cNr] = strdup("Unknown"); }
            }
            a = indexOf(m_chbuf, "upnp:albumArtURI", 0);
            if (a >= 0) {
                c = indexOf(m_chbuf, ">http", a);
                if (c >= 0) {
                    c += 1;
                    d = indexOf(m_chbuf, "<", c);
                    free(m_srvContent.albumArtURI[cNr]);
                    m_srvContent.albumArtURI[cNr] = x_ps_strndup(m_chbuf + c, d - c);
                    replacestr(m_srvContent.albumArtURI[cNr], "&amp;", "&");
                }
            }

            if (_browseResultCallback) {
                _browseResultCallback(m_srvContent.objectId[cNr], m_srvContent.parentId[cNr], m_srvContent.childCount[cNr],
                                       m_srvContent.title[cNr], m_srvContent.isAudio[cNr], m_srvContent.itemSize[cNr],
                                       m_srvContent.duration[cNr], m_srvContent.itemURL[cNr]);
            }
        }

        /*------ I T E M -------*/
        if (startsWith(content, "item id")) { item2 = true; memset(m_chbuf, 0, m_chbufSize); }
        if (item2) strcat(m_chbuf, content);
        if (startsWith(content, "/item")) {
            item2 = false;
            uint16_t cNr = m_srvContent.size;
            makeContentPushBack();

            replacestr(m_chbuf, "&quot", "\"");
            replacestr(m_chbuf, "&ampamp", "&");
            replacestr(m_chbuf, "&ampapos", "'");
            replacestr(m_chbuf, "&ampquot", "\"");
            replacestr(m_chbuf, "&lt", "<");
            replacestr(m_chbuf, "&gt", ">");

            a = indexOf(m_chbuf, "item id=", 0);
            if (a >= 0) {
                a += 9;
                b = indexOf(m_chbuf, "\"", a);
                free(m_srvContent.objectId[cNr]);
                m_srvContent.objectId[cNr] = x_ps_strndup(m_chbuf + a, b - a);
            }
            a = indexOf(m_chbuf, "parentID=", 0);
            if (a >= 0) {
                a += 10;
                b = indexOf(m_chbuf, "\"", a);
                free(m_srvContent.parentId[cNr]);
                m_srvContent.parentId[cNr] = x_ps_strndup(m_chbuf + a, b - a);
            }
            a = indexOf(m_chbuf, "object.item.audioItem", 0);
            m_srvContent.isAudio[cNr] = (a < 0) ? 0 : 1;

            a = indexOf(m_chbuf, "dc:title", 0);
            if (a >= 0) {
                a += 9;
                b = indexOf(m_chbuf, "/dc:title", a);
                b -= 1;
                free(m_srvContent.title[cNr]);
                m_srvContent.title[cNr] = x_ps_strndup(m_chbuf + a, b - a);
            }

            a = indexOf(m_chbuf, "dc:creator", 0);
            if (a >= 0) {
                a += 11;
                b = indexOf(m_chbuf, "/dc:creator", a);
                b -= 1;
                if (b > a) {
                    free(m_srvContent.artist[cNr]);
                    m_srvContent.artist[cNr] = x_ps_strndup(m_chbuf + a, b - a);
                }
            }
            a = indexOf(m_chbuf, "upnp:album>", 0); // "upnp:album>" excludes upnp:albumArtURI
            if (a >= 0) {
                a += 11;
                b = indexOf(m_chbuf, "/upnp:album>", a);
                b -= 1;
                if (b > a) {
                    free(m_srvContent.album[cNr]);
                    m_srvContent.album[cNr] = x_ps_strndup(m_chbuf + a, b - a);
                }
            }
            a = indexOf(m_chbuf, "upnp:albumArtURI", 0);
            if (a >= 0) {
                c = indexOf(m_chbuf, ">http", a);
                if (c >= 0) {
                    c += 1;
                    d = indexOf(m_chbuf, "<", c);
                    free(m_srvContent.albumArtURI[cNr]);
                    m_srvContent.albumArtURI[cNr] = x_ps_strndup(m_chbuf + c, d - c);
                    replacestr(m_srvContent.albumArtURI[cNr], "&amp;", "&");
                }
            }

            a = indexOf(m_chbuf, "<res", 0);
            b = indexOf(m_chbuf, "/res>", a);
            if (a > 0) {
                if (b > a) m_chbuf[b] = '\0';

                c = indexOf(m_chbuf, ">http", a);
                if (c >= 0) {
                    c += 1;
                    d = indexOf(m_chbuf, "<", c);
                    free(m_srvContent.itemURL[cNr]);
                    m_srvContent.itemURL[cNr] = x_ps_strndup(m_chbuf + c, d - c);
                }
                c = indexOf(m_chbuf, "duration=", a);
                if (c >= 0) {
                    c += 10;
                    d = indexOf(m_chbuf, "\"", c) - 4;
                    if (d > c) {
                        free(m_srvContent.duration[cNr]);
                        m_srvContent.duration[cNr] = x_ps_strndup(m_chbuf + c, d - c);
                    }
                }
                a = indexOf(m_chbuf, "size=", a);
                if (a > 0) {
                    a += 6;
                    b = indexOf(m_chbuf, "\"", a);
                    char tmp[60] = {0}; memcpy(tmp, m_chbuf + a, b - a);
                    m_srvContent.itemSize[cNr] = atol(tmp);
                }
            }

            if (_browseResultCallback) {
                _browseResultCallback(m_srvContent.objectId[cNr], m_srvContent.parentId[cNr], m_srvContent.childCount[cNr],
                                       m_srvContent.title[cNr], m_srvContent.isAudio[cNr], m_srvContent.itemSize[cNr],
                                       m_srvContent.duration[cNr], m_srvContent.itemURL[cNr]);
            }
        }

        if (startsWith(content, "<NumberReturned>")) {
            b = indexOf(content, "</NumberReturned>", 16);
            char tmp[10] = {0}; memcpy(tmp, content + 16, b - 16);
            m_numberReturned = atoi(tmp);
        }
        if (startsWith(content, "<TotalMatches>")) {
            b = indexOf(content, "</TotalMatches>", 14);
            char tmp[10] = {0}; memcpy(tmp, content + 14, b - 14);
            m_totalMatches = atoi(tmp);
        }
    }
    if (_browseReadyCallback) _browseReadyCallback(m_numberReturned, m_totalMatches);
    return true;
}
//------------------------------------------------------------------------------------------------
bool PrioDlnaClient::srvPost(uint8_t srvNr, const char* objectId, const uint16_t startingIndex, const uint16_t maxCount) {
    bool ret;
    uint8_t cnt = 0;

    m_client.stop();
    uint32_t t = millis();
    m_client.setTimeout(PRIO_DLNA_CONNECT_TIMEOUT);
    ret = m_client.connect(m_dlnaServer.ip[srvNr], m_dlnaServer.port[srvNr]);

    if (!ret) {
        m_client.stop();
        sprintf(m_chbuf, "The server %s:%d is not responding after %lums", m_dlnaServer.ip[srvNr], m_dlnaServer.port[srvNr], millis() - t);
        if (_infoCallback) _infoCallback(m_chbuf);
        return false;
    }
    while (true) {
        if (m_client.connected()) break;
        delay(100);
        cnt++;
        if (cnt == 10) {
            sprintf(m_chbuf, "The server %s:%d refuses the connection", m_dlnaServer.ip[srvNr], m_dlnaServer.port[srvNr]);
            if (_infoCallback) _infoCallback(m_chbuf);
            return false;
        }
    }

    // Build the SOAP body first and measure it, instead of patching a fixed-width
    // placeholder in the header afterwards - that patch only ever wrote exactly 3
    // digits, silently truncating/corrupting Content-Length for any body outside
    // the 100-999 byte range (e.g. once a server's ObjectID gets long enough).
    char body[512];
    snprintf(body, sizeof(body),
             "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
             "<s:Body>"
             "<u:Browse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">\r\n"
             "<ObjectID>%s</ObjectID>\r\n"
             "<BrowseFlag>BrowseDirectChildren</BrowseFlag>\r\n"
             "<Filter>*</Filter>\r\n"
             "<StartingIndex>%u</StartingIndex>\r\n"
             "<RequestedCount>%u</RequestedCount>\r\n"
             "<SortCriteria></SortCriteria>\r\n"
             "</u:Browse>\r\n"
             "</s:Body>\r\n"
             "</s:Envelope>\r\n",
             objectId, startingIndex, maxCount);

    // controlURL can come from the device description either as a bare relative
    // path or as an absolute "/..." path depending on the server vendor; always
    // emitting our own leading slash here avoids a "POST //..." double slash for
    // servers (e.g. Twonky) that give the absolute form.
    const char* path = m_dlnaServer.controlURL[srvNr];
    while (*path == '/') path++;

    sprintf(m_chbuf, "POST /%s HTTP/1.1\r\n"
                      "Host: %s:%d\r\n"
                      "CACHE-CONTROL: no-cache\r\nPRAGMA: no-cache\r\n"
                      "Connection: close\r\n"
                      "Content-Length: %u\r\n"
                      "Content-Type: text/xml; charset=\"utf-8\"\r\n"
                      "SOAPAction: \"urn:schemas-upnp-org:service:ContentDirectory:1#Browse\"\r\n"
                      "User-Agent: ESP32/Player/UPNP1.0\r\n"
                      "\r\n" /* end header, begin message */
                      "%s",
            path, m_dlnaServer.ip[srvNr], m_dlnaServer.port[srvNr], (unsigned)strlen(body), body);

    m_client.print(m_chbuf);

    t = millis() + PRIO_DLNA_AVAIL_TIMEOUT;
    while (true) {
        if (m_client.available()) break;
        if (t < millis()) {
            sprintf(m_chbuf, "The server %s:%d is not responding after request", m_dlnaServer.ip[srvNr], m_dlnaServer.port[srvNr]);
            if (_infoCallback) _infoCallback(m_chbuf);
            return false;
        }
    }
    return true;
}
//------------------------------------------------------------------------------------------------
int8_t PrioDlnaClient::browseServer(uint8_t srvNr, const char* objectId, const uint16_t startingIndex, const uint16_t maxCount) {
    if (!objectId) { log_e("objectId is NULL"); return -1; }
    if (srvNr >= m_dlnaServer.size) { log_e("server index too high"); return -2; }
    if (m_state != IDLE) { log_e("state is not idle"); return -3; }

    m_srvNr = srvNr;
    strncpy(m_objectId, objectId, sizeof(m_objectId) - 1);
    m_objectId[sizeof(m_objectId) - 1] = '\0';
    m_startingIndex = startingIndex;
    m_maxCount = maxCount;
    m_state = BROWSE_SERVER;
    return 0;
}
//------------------------------------------------------------------------------------------------
const char* PrioDlnaClient::stringifyServer() {
    if (m_dlnaServer.size == 0) return "[]";

    uint16_t JSONstrLength = 0;
    if (m_JSONstr) { free(m_JSONstr); m_JSONstr = nullptr; }
    if (m_PSRAMfound) { m_JSONstr = (char*)ps_malloc(2); }
    else { m_JSONstr = (char*)malloc(2); }
    JSONstrLength += 2;
    memcpy(m_JSONstr, "[\0", 2);

    char id[5]; char port[6];

    for (int i = 0; i < m_dlnaServer.size; i++) {
        itoa(i, id, 10);
        itoa(m_dlnaServer.port[i], port, 10);
        JSONstrLength = strlen(id) + strlen(port) + strlen(m_dlnaServer.friendlyName[i]) + strlen(m_dlnaServer.ip[i]);
        JSONstrLength += strlen(m_JSONstr) + 49 + 2;

        if (m_PSRAMfound) { m_JSONstr = (char*)ps_realloc(m_JSONstr, JSONstrLength); }
        else { m_JSONstr = (char*)realloc(m_JSONstr, JSONstrLength); }

        strcat(m_JSONstr, "{\"srvId\":\""); strcat(m_JSONstr, id);
        strcat(m_JSONstr, "\",\"friendlyName\":\""); strcat(m_JSONstr, m_dlnaServer.friendlyName[i]);
        strcat(m_JSONstr, "\",\"ip\":\""); strcat(m_JSONstr, m_dlnaServer.ip[i]);
        strcat(m_JSONstr, "\",\"port\":\""); strcat(m_JSONstr, port);
        strcat(m_JSONstr, "\"},");
    }
    m_JSONstr[JSONstrLength - 3] = ']';
    m_JSONstr[JSONstrLength - 2] = '\0';
    return m_JSONstr;
}
//------------------------------------------------------------------------------------------------
const char* PrioDlnaClient::stringifyContent() {
    if (m_srvContent.size == 0) return "[]";

    uint16_t JSONstrLength = 0;
    if (m_JSONstr) { free(m_JSONstr); m_JSONstr = nullptr; }
    if (m_PSRAMfound) { m_JSONstr = (char*)ps_malloc(2); }
    else { m_JSONstr = (char*)malloc(2); }
    JSONstrLength += 2;
    memcpy(m_JSONstr, "[\0", 2);

    char childCount[5]; char isAudio[6]; char itemSize[12];

    for (int i = 0; i < m_srvContent.size; i++) {
        itoa(m_srvContent.childCount[i], childCount, 10);
        strcpy(isAudio, m_srvContent.isAudio[i] ? "true" : "false");
        ltoa(m_srvContent.itemSize[i], itemSize, 10);
        JSONstrLength = strlen(childCount) + strlen(isAudio) + strlen(itemSize) + strlen(m_srvContent.itemURL[i]) +
                        strlen(m_srvContent.objectId[i]) + strlen(m_srvContent.parentId[i]) + strlen(m_srvContent.title[i]) + strlen(m_srvContent.duration[i]);
        JSONstrLength += strlen(m_JSONstr) + 105 + 2;

        if (m_PSRAMfound) { m_JSONstr = (char*)ps_realloc(m_JSONstr, JSONstrLength); }
        else { m_JSONstr = (char*)realloc(m_JSONstr, JSONstrLength); }

        strcat(m_JSONstr, "{\"objectId\":\""); strcat(m_JSONstr, m_srvContent.objectId[i]);
        strcat(m_JSONstr, "\",\"parentId\":\""); strcat(m_JSONstr, m_srvContent.parentId[i]);
        strcat(m_JSONstr, "\",\"childCount\":\""); strcat(m_JSONstr, childCount);
        strcat(m_JSONstr, "\",\"title\":\""); strcat(m_JSONstr, m_srvContent.title[i]);
        strcat(m_JSONstr, "\",\"isAudio\":\""); strcat(m_JSONstr, isAudio);
        strcat(m_JSONstr, "\",\"itemSize\":\""); strcat(m_JSONstr, itemSize);
        strcat(m_JSONstr, "\",\"dur\":\""); strcat(m_JSONstr, m_srvContent.duration[i]);
        strcat(m_JSONstr, "\",\"itemURL\":\""); strcat(m_JSONstr, m_srvContent.itemURL[i]);
        strcat(m_JSONstr, "\"},");
    }
    m_JSONstr[JSONstrLength - 2] = ']';
    m_JSONstr[JSONstrLength - 1] = '\0';
    return m_JSONstr;
}
//------------------------------------------------------------------------------------------------
uint8_t PrioDlnaClient::getState() {
    return (uint8_t)m_state;
}
//------------------------------------------------------------------------------------------------
void PrioDlnaClient::loop() {
    static uint8_t cnt = 0;
    static uint8_t fail = 0;
    bool res;
    switch (m_state) {
        case IDLE:
            break;
        case SEEK_SERVER:
            if (m_timeStamp + PRIO_DLNA_SEEK_TIMEOUT > millis()) {
                int len = m_udp.parsePacket();
                if (len > 0) parseDlnaServer(len); // registers all media servers that respond until timeout
                cnt = 0;
                fail = 0;
            } else {
                m_udp.stop();
                m_state = GET_SERVER_ITEMS;
            }
            break;
        case GET_SERVER_ITEMS:
            if (cnt < m_dlnaServer.size) {
                if (fail == 3) { fail = 0; log_e("no response from svr [%i]", cnt); cnt++; break; }
                res = srvGet(cnt);
                if (!res) { fail++; break; }
                res = readHttpHeader();
                if (!res) { fail++; break; }
                res = readContent();
                if (!res) { fail++; break; }
                res = getServerItems(cnt);
                if (!res) { fail++; break; }
                cnt++;
                break;
            }
            cnt = 0;
            // Only overwrite a previously good cache when this scan actually found
            // something - a transient scan failure shouldn't wipe out servers that
            // were reachable moments ago and may well be reachable again next time.
            if (m_dlnaServer.size > 0) saveServerCache();
            if (_seekReadyCallback) _seekReadyCallback(m_dlnaServer.size);
            m_state = IDLE;
            break;
        case BROWSE_SERVER:
            res = srvPost(m_srvNr, m_objectId, m_startingIndex, m_maxCount);
            if (!res) { m_state = IDLE; break; }
            res = readHttpHeader();
            if (!res) { m_state = IDLE; break; }
            res = readContent();
            if (!res) { m_state = IDLE; break; }
            res = browseResult();
            if (!res) { m_state = IDLE; break; }
            cnt = 0;
            m_state = IDLE;
            break;
        default:
            break;
    }
}
//------------------------------------------------------------------------------------------------
void PrioDlnaClient::saveServerCache() {
    File f = LittleFS.open(DLNA_SERVER_CACHE_FILE, "w");
    if (!f) { log_e("could not open %s for writing", DLNA_SERVER_CACHE_FILE); return; }
    for (int i = 0; i < m_dlnaServer.size; i++) {
        f.printf("%s|%u|%s|%s|%s|%u|%s\n",
                 m_dlnaServer.ip[i], m_dlnaServer.port[i], m_dlnaServer.location[i],
                 m_dlnaServer.friendlyName[i], m_dlnaServer.controlURL[i],
                 m_dlnaServer.presentationPort[i], m_dlnaServer.presentationURL[i]);
    }
    f.close();
}

bool PrioDlnaClient::loadServerCache() {
    if (!LittleFS.exists(DLNA_SERVER_CACHE_FILE)) return false;
    File f = LittleFS.open(DLNA_SERVER_CACHE_FILE, "r");
    if (!f) return false;

    dlnaServer_clear_and_shrink();
    m_dlnaServer.size = 0;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        String fields[7];
        int fieldCount = 0;
        int fieldStart = 0;
        for (int i = 0; i <= (int)line.length() && fieldCount < 7; i++) {
            if (i == (int)line.length() || line[i] == '|') {
                fields[fieldCount++] = line.substring(fieldStart, i);
                fieldStart = i + 1;
            }
        }
        if (fieldCount != 7) { log_e("skipping malformed cache line"); continue; }

        m_dlnaServer.ip.push_back(x_ps_strdup(fields[0].c_str()));
        m_dlnaServer.port.push_back((uint16_t)fields[1].toInt());
        m_dlnaServer.location.push_back(x_ps_strdup(fields[2].c_str()));
        m_dlnaServer.friendlyName.push_back(x_ps_strdup(fields[3].c_str()));
        m_dlnaServer.controlURL.push_back(x_ps_strdup(fields[4].c_str()));
        m_dlnaServer.presentationPort.push_back((uint16_t)fields[5].toInt());
        m_dlnaServer.presentationURL.push_back(x_ps_strdup(fields[6].c_str()));
        m_dlnaServer.size++;
    }
    f.close();
    return m_dlnaServer.size > 0;
}
//------------------------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------------------------
void PrioDlnaClient::vector_clear_and_shrink(std::vector<char*> &vec) {
    for (size_t i = 0; i < vec.size(); i++) {
        if (vec[i]) { free(vec[i]); vec[i] = nullptr; }
    }
    vec.clear();
    vec.shrink_to_fit();
}

void PrioDlnaClient::dlnaServer_clear_and_shrink() {
    m_dlnaServer.size = 0;
    vector_clear_and_shrink(m_dlnaServer.ip);
    m_dlnaServer.port.clear(); m_dlnaServer.port.shrink_to_fit();
    vector_clear_and_shrink(m_dlnaServer.location);
    vector_clear_and_shrink(m_dlnaServer.friendlyName);
    vector_clear_and_shrink(m_dlnaServer.controlURL);
    m_dlnaServer.presentationPort.clear(); m_dlnaServer.presentationPort.shrink_to_fit();
    vector_clear_and_shrink(m_dlnaServer.presentationURL);
}

void PrioDlnaClient::srvContent_clear_and_shrink() {
    m_srvContent.size = 0;
    vector_clear_and_shrink(m_srvContent.objectId);
    vector_clear_and_shrink(m_srvContent.parentId);
    m_srvContent.isAudio.clear(); m_srvContent.isAudio.shrink_to_fit();
    vector_clear_and_shrink(m_srvContent.itemURL);
    m_srvContent.itemSize.clear(); m_srvContent.itemSize.shrink_to_fit();
    vector_clear_and_shrink(m_srvContent.duration);
    vector_clear_and_shrink(m_srvContent.title);
    m_srvContent.childCount.clear(); m_srvContent.childCount.shrink_to_fit();
    vector_clear_and_shrink(m_srvContent.artist);
    vector_clear_and_shrink(m_srvContent.album);
    vector_clear_and_shrink(m_srvContent.albumArtURI);
}

int32_t PrioDlnaClient::indexOf(const char* haystack, const char* needle, int32_t startIndex) {
    const char* p = haystack;
    for (; startIndex > 0; startIndex--)
        if (*p++ == '\0') return -1;
    char* pos = strstr((char*)p, needle);
    if (pos == nullptr) return -1;
    return pos - haystack;
}

bool PrioDlnaClient::startsWith(const char* base, const char* searchString) {
    char c;
    while ((c = *searchString++) != '\0')
        if (c != *base++) return false;
    return true;
}

bool PrioDlnaClient::endsWith(const char* base, const char* searchString) {
    int32_t slen = strlen(searchString);
    if (slen == 0) return false;
    const char* p = base + strlen(base);
    p -= slen;
    if (p < base) return false;
    return (strncmp(p, searchString, slen) == 0);
}

int PrioDlnaClient::replacestr(char* line, const char* search, const char* replace) {
    char* sp;
    if ((sp = strstr(line, search)) == nullptr) return 0;
    int count = 1;
    int sLen = strlen(search);
    int rLen = strlen(replace);
    if (sLen > rLen) {
        char* src = sp + sLen;
        char* dst = sp + rLen;
        while ((*dst = *src) != '\0') { dst++; src++; }
    } else if (sLen < rLen) {
        int tLen = strlen(sp) - sLen;
        char* stop = sp + rLen;
        char* src = sp + sLen + tLen;
        char* dst = sp + rLen + tLen;
        while (dst >= stop) { *dst = *src; dst--; src--; }
    }
    memcpy(sp, replace, rLen);
    count += replacestr(sp + rLen, search, replace);
    return count;
}

char PrioDlnaClient::toLowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

char* PrioDlnaClient::x_ps_malloc(uint16_t len) {
    char* ps_str = nullptr;
    if (psramFound()) ps_str = (char*)ps_malloc(len);
    else ps_str = (char*)malloc(len);
    if (!ps_str) { log_e("oom"); return nullptr; }
    ps_str[0] = '\0';
    return ps_str;
}

char* PrioDlnaClient::x_ps_strdup(const char* str) {
    if (!str) { log_e("given str is NULL"); return nullptr; }
    uint16_t len = strlen(str);
    char* ps_str = m_PSRAMfound ? (char*)ps_malloc(len + 1) : (char*)malloc(len + 1);
    if (!ps_str) { log_e("oom"); return nullptr; }
    strcpy(ps_str, str);
    ps_str[len] = '\0';
    return ps_str;
}

char* PrioDlnaClient::x_ps_strndup(const char* str, uint16_t len) {
    if (!str) { log_e("given str is NULL"); return nullptr; }
    size_t str_len = strlen(str);
    if (len > str_len) len = str_len;
    char* ps_str = m_PSRAMfound ? (char*)ps_malloc(len + 1) : (char*)malloc(len + 1);
    if (!ps_str) { log_e("oom"); return nullptr; }
    strlcpy(ps_str, str, len + 1); // len+1 guarantees zero termination
    return ps_str;
}
