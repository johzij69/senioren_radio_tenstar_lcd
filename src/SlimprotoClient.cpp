#include "SlimprotoClient.h"

SlimprotoClient::SlimprotoClient(Audio* audioPtr) {
    audio = audioPtr;
    connected = false;
    playing = false;
    streaming = false;
    httpConnected = false;
    volume = 75;
    audioFormat = SLIMPROTO_PCM;
    sampleRate = 44100;
    channels = 2;
    bufferPos = 0;
    bytesReceived = 0;
    lastHeartbeat = 0;
    lastStatUpdate = 0;
    connectTime = 0;
    playerName = "ESP32-Squeezebox";
    memset(macAddress, 0, 6);
}

SlimprotoClient::~SlimprotoClient() {
    disconnect();
}

void SlimprotoClient::setMACAddress(uint8_t* mac) {
    memcpy(macAddress, mac, 6);
}

void SlimprotoClient::setPlayerName(const char* name) {
    playerName = String(name);
}

bool SlimprotoClient::begin(const char* ip, uint16_t port) {
    serverIP = String(ip);
    serverPort = port;
    
    // Haal MAC adres op als niet ingesteld
    if (macAddress[0] == 0 && macAddress[1] == 0) {
        WiFi.macAddress(macAddress);
    }
    
    return connectToServer();
}

bool SlimprotoClient::connectToServer() {
    Serial.println("[Slimproto] Verbinden met LMS server...");
    
    if (tcpClient.connect(serverIP.c_str(), serverPort)) {
        Serial.println("[Slimproto] Verbonden met LMS server");
        connected = true;
        connectTime = millis();
        sendHelo();
        sendStat("STMc"); // Connection established
        return true;
    }
    
    Serial.println("[Slimproto] Verbinding mislukt");
    connected = false;
    return false;
}

void SlimprotoClient::disconnect() {
    if (httpConnected) {
        disconnectHTTPStream();
    }
    if (connected) {
        tcpClient.stop();
        connected = false;
    }
    playing = false;
    streaming = false;
}

void SlimprotoClient::sendHelo() {
    uint8_t packet[44];
    memset(packet, 0, sizeof(packet));
    
    // Opcode "HELO"
    write32BE(packet, SLIMPROTO_HELO);
    
    // Device ID (12 = Squeezebox Boom, werkt goed voor audio)
    packet[4] = 12;
    
    // Revisie
    packet[5] = 0;
    
    // MAC adres
    memcpy(packet + 6, macAddress, 6);
    
    // UUID (gebruik MAC als basis + random)
    for (int i = 0; i < 16; i++) {
        packet[12 + i] = macAddress[i % 6] ^ (i * 17);
    }
    
    // WAN/LAN flags (LAN)
    write16BE(packet + 28, 0);
    
    // Bytes ontvangen sinds opstart
    write32BE(packet + 30, 0);
    
    // Language code (leeg)
    write16BE(packet + 34, 0);
    
    tcpClient.write(packet, sizeof(packet));
    Serial.println("[Slimproto] HELO verzonden");
}

void SlimprotoClient::sendStat(const char* event) {
    if (!connected) return;
    
    uint8_t packet[1024];
    memset(packet, 0, sizeof(packet));
    
    int pos = 0;
    
    // Opcode "STAT"
    write32BE(packet + pos, SLIMPROTO_STAT);
    pos += 4;
    
    // Lengte (vullen we later in)
    int lengthPos = pos;
    pos += 4;
    
    // Event code (4 chars)
    memcpy(packet + pos, event, 4);
    pos += 4;
    
    // Aantal crlf
    packet[pos++] = 0;
    
    // Mas initialized
    packet[pos++] = (streaming ? 1 : 0);
    
    // Mas mode (autonomous)
    packet[pos++] = 0;
    
    // Streaming buffer size
    write32BE(packet + pos, STREAM_BUFFER_SIZE);
    pos += 4;
    
    // Fullness (hoeveel data in buffer)
    write32BE(packet + pos, bufferPos);
    pos += 4;
    
    // Bytes ontvangen sinds stream start
    write32BE(packet + pos, bytesReceived);
    pos += 4;
    
    // Signal strength (wifi RSSI)
    int16_t rssi = WiFi.RSSI();
    write16BE(packet + pos, abs(rssi));
    pos += 2;
    
    // Timestamp (jiffies, 1kHz ticks)
    uint32_t jiffies = millis();
    write32BE(packet + pos, jiffies);
    pos += 4;
    
    // Output buffer size
    write32BE(packet + pos, 0);
    pos += 4;
    
    // Output buffer fullness
    write32BE(packet + pos, 0);
    pos += 4;
    
    // Elapsed seconds
    uint32_t elapsed = streaming ? (millis() - connectTime) / 1000 : 0;
    write32BE(packet + pos, elapsed);
    pos += 4;
    
    // Voltage (niet gebruikt op ESP32)
    write16BE(packet + pos, 0);
    pos += 2;
    
    // Elapsed milliseconds
    write32BE(packet + pos, streaming ? (millis() - connectTime) : 0);
    pos += 4;
    
    // Server timestamp
    write32BE(packet + pos, 0);
    pos += 4;
    
    // Error code
    write16BE(packet + pos, 0);
    pos += 2;
    
    // Vul lengte in
    write32BE(packet + lengthPos, pos - 8);
    
    tcpClient.write(packet, pos);
    lastStatUpdate = millis();
    
    Serial.printf("[Slimproto] STAT verzonden: %s\n", event);
}

void SlimprotoClient::loop() {
    // Probeer te reconnecten als niet verbonden
    if (!connected) {
        static unsigned long lastRetry = 0;
        if (millis() - lastRetry > 5000) {
            lastRetry = millis();
            connectToServer();
        }
        return;
    }
    
    // Check of TCP verbinding nog actief is
    if (!tcpClient.connected()) {
        Serial.println("[Slimproto] TCP verbinding verloren");
        disconnect();
        return;
    }
    
    // Ontvang en verwerk control packets van LMS
    while (tcpClient.available() >= 8) {
        if (!receivePacket()) {
            delay(1);
            break;
        }
    }
    
    // Verwerk HTTP audio stream
    if (streaming && httpConnected) {
        processHTTPStream();
    }
    
    // Stuur periodieke status updates
    if (streaming && (millis() - lastStatUpdate > 1000)) {
        sendStat("STMt"); // Timer update
    }
    
    // Audio library loop moet ook draaien
    if (audio && streaming) {
        audio->loop();
    }
}

bool SlimprotoClient::receivePacket() {
    if (tcpClient.available() < 8) {
        return false;
    }
    
    // Lees header
    uint8_t header[8];
    tcpClient.readBytes(header, 8);
    
    uint32_t opcode = read32BE(header);
    uint32_t length = read32BE(header + 4);
    
    // Begrens lengte
    if (length > MAX_PACKET_SIZE) {
        Serial.printf("[Slimproto] Packet te groot: %d bytes, overslaan\n", length);
        while (length > 0 && tcpClient.available() > 0) {
            tcpClient.read();
            length--;
        }
        return false;
    }
    
    // Lees payload
    uint8_t payload[MAX_PACKET_SIZE];
    if (length > 0) {
        size_t bytesRead = 0;
        unsigned long timeout = millis();
        
        while (bytesRead < length && (millis() - timeout < 1000)) {
            if (tcpClient.available()) {
                payload[bytesRead++] = tcpClient.read();
            } else {
                delay(1);
            }
        }
        
        if (bytesRead != length) {
            Serial.printf("[Slimproto] Incomplete packet: %d/%d bytes\n", bytesRead, length);
            return false;
        }
    }
    
    // Verwerk packet op basis van opcode
    switch (opcode) {
        case SLIMPROTO_STRM:
            handleStrm(payload, length);
            break;
        case SLIMPROTO_SERV:
            handleServ(payload, length);
            break;
        default:
            // Onbekend packet
            break;
    }
    
    return true;
}

void SlimprotoClient::handleStrm(uint8_t* data, size_t len) {
    if (len < 24) return;
    
    char command = data[0];
    
    Serial.printf("[Slimproto] STRM command: %c\n", command);
    
    switch (command) {
        case 's': { // Start streaming
            audioFormat = data[4];
            
            // Sample rate (3 bytes, big endian, scaled by 1)
            sampleRate = (data[11] << 16) | (data[12] << 8) | data[13];
            
            // Extract HTTP URL from data
            String url = extractStreamUrl(data, len);
            
            Serial.printf("[Slimproto] Stream start\n");
            Serial.printf("  Format: 0x%02X\n", audioFormat);
            Serial.printf("  Sample rate: %d Hz\n", sampleRate);
            Serial.printf("  URL: %s\n", url.c_str());
            
            // Stop oude stream
            if (httpConnected) {
                disconnectHTTPStream();
            }
            if (audio) {
                audio->stopSong();
            }
            
            // Start nieuwe stream via Audio library
            if (audio && url.length() > 0) {
                if (audio->connecttohost(url.c_str())) {
                    streaming = true;
                    playing = true;
                    httpConnected = true;
                    bytesReceived = 0;
                    connectTime = millis();
                    
                    // Zet volume
                    audio->setVolume(map(volume, 0, 100, 0, 21));
                    
                    sendStat("STMf"); // Flushed and ready
                } else {
                    Serial.println("[Slimproto] Kan niet verbinden met stream URL");
                    sendStat("STMd"); // Done/stopped
                }
            }
            break;
        }
        
        case 'p': // Pause
            playing = false;
            if (audio) {
                audio->pauseResume();
            }
            Serial.println("[Slimproto] Gepauzeerd");
            sendStat("STMp");
            break;
        
        case 'u': // Unpause
            playing = true;
            if (audio) {
                audio->pauseResume();
            }
            Serial.println("[Slimproto] Hervat");
            sendStat("STMr");
            break;
        
        case 'q': // Quit/Stop
            playing = false;
            streaming = false;
            if (audio) {
                audio->stopSong();
            }
            disconnectHTTPStream();
            bytesReceived = 0;
            Serial.println("[Slimproto] Gestopt");
            sendStat("STMd");
            break;
        
        case 't': // Status request
            sendStat("STMt");
            break;
        
        case 'f': // Flush
            if (audio) {
                audio->stopSong();
            }
            bufferPos = 0;
            sendStat("STMf");
            break;
    }
}

void SlimprotoClient::handleServ(uint8_t* data, size_t len) {
    // Server assignment
    Serial.println("[Slimproto] Server assignment ontvangen");
    
    // Optioneel: parse IP adres van nieuwe server
    // Voor nu accepteren we assignment
}

String SlimprotoClient::extractStreamUrl(uint8_t* data, size_t len) {
    // STRM packet structuur:
    // byte 0: command ('s')
    // byte 1: autostart
    // byte 2: format byte
    // byte 3: pcm sample size
    // byte 4: pcm sample rate
    // byte 5: pcm channels
    // byte 6: pcm endian
    // byte 7: threshold
    // byte 8: spdif enable
    // byte 9: trans period
    // byte 10: trans type
    // byte 11-13: sample rate (24 bit BE)
    // byte 14-17: replay gain
    // byte 18: output threshold
    // byte 19: flags
    // byte 20: reserved
    // byte 21+: HTTP headers (inclusief GET URL)
    
    if (len < 24) return "";
    
    // Zoek naar HTTP GET request in data
    String headers = "";
    for (size_t i = 21; i < len; i++) {
        if (data[i] == 0) break;
        headers += (char)data[i];
    }
    
    // Parse URL uit GET request
    // Format: "GET /stream HTTP/1.0\r\n..."
    int getPos = headers.indexOf("GET ");
    if (getPos >= 0) {
        int pathStart = getPos + 4;
        int pathEnd = headers.indexOf(" HTTP", pathStart);
        if (pathEnd > pathStart) {
            String path = headers.substring(pathStart, pathEnd);
            
            // Zoek naar Host header
            int hostPos = headers.indexOf("Host: ");
            if (hostPos >= 0) {
                int hostStart = hostPos + 6;
                int hostEnd = headers.indexOf("\r\n", hostStart);
                if (hostEnd < 0) hostEnd = headers.indexOf("\n", hostStart);
                if (hostEnd > hostStart) {
                    String host = headers.substring(hostStart, hostEnd);
                    host.trim();
                    
                    // Bouw volledige URL
                    String url = "http://" + host + path;
                    return url;
                }
            }
        }
    }
    
    return "";
}

void SlimprotoClient::processHTTPStream() {
    // De Audio library handelt het streamen zelf af in audio->loop()
    // We hoeven hier alleen de bytes bij te houden voor status
    
    // Dit is een schatting - de Audio library houdt zijn eigen administratie bij
    bytesReceived += 1024; // Geschatte waarde per loop iteratie
}

void SlimprotoClient::disconnectHTTPStream() {
    if (httpConnected) {
        if (audio) {
            audio->stopSong();
        }
        httpConnected = false;
        Serial.println("[Slimproto] HTTP stream afgesloten");
    }
}

void SlimprotoClient::setVolume(uint8_t vol) {
    volume = constrain(vol, 0, 100);
    if (audio) {
        // Audio library gebruikt 0-21 schaal
        audio->setVolume(map(volume, 0, 100, 0, 21));
    }
    Serial.printf("[Slimproto] Volume: %d%%\n", volume);
}

bool SlimprotoClient::isConnected() {
    return connected;
}

bool SlimprotoClient::isPlaying() {
    return playing && streaming;
}

uint8_t SlimprotoClient::getVolume() {
    return volume;
}

// Hulpfuncties
uint32_t SlimprotoClient::read32BE(uint8_t* data) {
    return ((uint32_t)data[0] << 24) | 
           ((uint32_t)data[1] << 16) | 
           ((uint32_t)data[2] << 8) | 
           data[3];
}

uint16_t SlimprotoClient::read16BE(uint8_t* data) {
    return ((uint16_t)data[0] << 8) | data[1];
}

void SlimprotoClient::write32BE(uint8_t* data, uint32_t value) {
    data[0] = (value >> 24) & 0xFF;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;
}

void SlimprotoClient::write16BE(uint8_t* data, uint16_t value) {
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;
}