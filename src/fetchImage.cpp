#include "fetchImage.h"
// Fetch a file from the URL given and save it in LittleFS.
// Return true if the file is now present locally (already cached, or freshly downloaded).

#define FORMAT_LITTLEFS_IF_FAILED false

// Only plain http:// is supported. https:// would need WiFiClientSecure, whose
// BearSSL context previously destabilized the Audio library's own TLS stream
// connections when both ran concurrently - this project intentionally sticks
// to a bare WiFiClient (same approach as PrioDlnaClient's SOAP GET/POST) so
// image downloads never touch that shared TLS state. DLNA media servers serve
// album art over plain http on the LAN, so this covers that use case.
bool getFile(String url, String filename)
{
    if (!url.startsWith("http://")) {
        Serial.println("[ERROR] getFile: only http:// URLs are supported: " + url);
        return false;
    }

    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("[ERROR] LittleFS initialization failed!");
        return false;
    }

    if (LittleFS.exists(filename)) {
        Serial.println("[INFO] File already exists: " + filename);
        return true;
    }

    String rest = url.substring(7); // strip "http://"
    int slashIdx = rest.indexOf('/');
    String hostPort = (slashIdx >= 0) ? rest.substring(0, slashIdx) : rest;
    String path = (slashIdx >= 0) ? rest.substring(slashIdx) : "/";
    String host = hostPort;
    uint16_t port = 80;
    int colonIdx = hostPort.indexOf(':');
    if (colonIdx >= 0) {
        host = hostPort.substring(0, colonIdx);
        port = (uint16_t)hostPort.substring(colonIdx + 1).toInt();
    }

    WiFiClient client;
    client.setTimeout(5000);
    if (!client.connect(host.c_str(), port)) {
        Serial.printf("[ERROR] getFile: could not connect to %s:%u\n", host.c_str(), port);
        return false;
    }

    client.printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: ESP32/Player/1.0\r\n\r\n",
                  path.c_str(), host.c_str());

    unsigned long deadline = millis() + 5000;
    while (client.connected() && !client.available()) {
        if (millis() > deadline) {
            Serial.println("[ERROR] getFile: timeout waiting for response from " + host);
            client.stop();
            return false;
        }
        delay(5);
    }

    String statusLine = client.readStringUntil('\n');
    if (statusLine.indexOf(" 200 ") < 0) {
        Serial.println("[ERROR] getFile: unexpected HTTP status from " + url + ": " + statusLine);
        client.stop();
        return false;
    }

    long contentLength = -1;
    bool chunked = false;
    while (client.connected() || client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) break; // blank line ends the header block
        String lower = line;
        lower.toLowerCase();
        if (lower.startsWith("content-length:")) {
            contentLength = line.substring(line.indexOf(':') + 1).toInt();
        } else if (lower.startsWith("transfer-encoding:") && lower.indexOf("chunked") >= 0) {
            chunked = true;
        }
    }

    if (chunked) {
        Serial.println("[ERROR] getFile: chunked transfer-encoding not supported: " + url);
        client.stop();
        return false;
    }

    File file = LittleFS.open(filename, "w");
    if (!file) {
        Serial.println("[ERROR] getFile: could not open for writing: " + filename);
        client.stop();
        return false;
    }

    uint8_t buf[512];
    long remaining = contentLength; // -1 means "unknown, read until the connection closes"
    unsigned long lastData = millis();
    while (client.connected() || client.available()) {
        int avail = client.available();
        if (avail > 0) {
            int toRead = sizeof(buf);
            if (avail < toRead) toRead = avail;
            if (remaining >= 0 && remaining < toRead) toRead = (int)remaining;
            int n = client.read(buf, toRead);
            if (n > 0) {
                file.write(buf, n);
                if (remaining >= 0) remaining -= n;
                lastData = millis();
            }
        } else {
            if (millis() - lastData > 5000) {
                Serial.println("[ERROR] getFile: stalled download for " + url);
                break;
            }
            delay(2);
        }
        if (remaining == 0) break;
    }
    file.close();
    client.stop();

    if (remaining > 0) { // known length that we didn't fully receive
        Serial.println("[ERROR] getFile: incomplete download for " + url);
        LittleFS.remove(filename);
        return false;
    }

    Serial.println("[INFO] getFile: downloaded " + url + " -> " + filename);
    return true;
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.printf("  DIR : %s\n", file.name());
            if (levels) {
                listDir(fs, file.name(), levels - 1);
            }
        } else {
            Serial.printf("  FILE: %s  SIZE: %d\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
}
