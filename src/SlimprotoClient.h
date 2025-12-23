#ifndef SLIMPROTO_CLIENT_H
#define SLIMPROTO_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include "Audio.h"

// Slimproto opdracht types
#define SLIMPROTO_STRM 0x7374726D  // 'strm' - stream control
#define SLIMPROTO_STAT 0x73746174  // 'stat' - status update
#define SLIMPROTO_SERV 0x73657276  // 'serv' - server assignment
#define SLIMPROTO_HELO 0x68656C6F  // 'helo' - hello/discovery
#define SLIMPROTO_DSCO 0x6473636F  // 'dsco' - disconnect

// Audio formaten
#define SLIMPROTO_PCM 0x70
#define SLIMPROTO_MP3 0x6D
#define SLIMPROTO_AAC 0x61
#define SLIMPROTO_OGG 0x6F
#define SLIMPROTO_FLC 0x66

// Buffer groottes
#define STREAM_BUFFER_SIZE 8192
#define MAX_PACKET_SIZE 2048

class SlimprotoClient {
public:
    SlimprotoClient(Audio* audioPtr);
    ~SlimprotoClient();
    
    // Initialisatie
    bool begin(const char* serverIP, uint16_t serverPort = 3483);
    void setMACAddress(uint8_t* mac);
    void setPlayerName(const char* name);
    
    // Hoofdloop - MOET regelmatig worden aangeroepen!
    void loop();
    
    // Status
    bool isConnected();
    bool isPlaying();
    uint8_t getVolume();
    
    // Volume (0-100)
    void setVolume(uint8_t volume);

private:
    // Audio object
    Audio* audio;
    
    // Netwerk
    WiFiClient tcpClient;
    String serverIP;
    uint16_t serverPort;
    bool connected;
    
    // Device info
    uint8_t macAddress[6];
    String playerName;
    
    // Status
    bool playing;
    bool streaming;
    uint8_t volume;
    uint8_t audioFormat;
    uint32_t sampleRate;
    uint8_t channels;
    
    // HTTP stream voor audio data
    WiFiClient httpClient;
    String streamUrl;
    bool httpConnected;
    
    // Buffers
    uint8_t streamBuffer[STREAM_BUFFER_SIZE];
    uint16_t bufferPos;
    uint32_t bytesReceived;
    
    // Timing
    unsigned long lastHeartbeat;
    unsigned long lastStatUpdate;
    unsigned long connectTime;
    
    // Protocol functies
    bool connectToServer();
    void disconnect();
    void sendHelo();
    void sendStat(const char* event);
    bool receivePacket();
    void processPacket(uint8_t* data, size_t len);
    
    // Packet handlers
    void handleStrm(uint8_t* data, size_t len);
    void handleServ(uint8_t* data, size_t len);
    
    // HTTP streaming
    bool connectHTTPStream(const char* url);
    void processHTTPStream();
    void disconnectHTTPStream();
    
    // Hulpfuncties
    uint32_t read32BE(uint8_t* data);
    uint16_t read16BE(uint8_t* data);
    void write32BE(uint8_t* data, uint32_t value);
    void write16BE(uint8_t* data, uint16_t value);
    String extractStreamUrl(uint8_t* data, size_t len);
};

#endif // SLIMPROTO_CLIENT_H