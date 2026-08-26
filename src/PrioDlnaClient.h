#ifndef PRIO_DLNA_CLIENT_H
#define PRIO_DLNA_CLIENT_H

// DLNA/UPnP media server discovery and browsing client.
// Adapted from https://github.com/schreibfaul1/ESP32-DLNA-Client for this project's
// callback conventions (instance callbacks instead of weak globals, so multiple
// modules can each register their own handlers without symbol collisions).

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <vector>

#define PRIO_DLNA_MULTICAST_IP     239, 255, 255, 250
#define PRIO_DLNA_LOCAL_PORT       8888
#define PRIO_DLNA_MULTICAST_PORT   1900
#define PRIO_DLNA_SEEK_TIMEOUT     8000
#define PRIO_DLNA_READ_TIMEOUT     2500
#define PRIO_DLNA_CONNECT_TIMEOUT  6000
#define PRIO_DLNA_AVAIL_TIMEOUT    2000

class PrioDlnaClient {
public:
    typedef struct _dlnaServer {
        uint16_t size = 0;
        std::vector<char*>     ip;
        std::vector<uint16_t>  port;
        std::vector<char*>     location;
        std::vector<char*>     friendlyName;
        std::vector<char*>     controlURL;
        std::vector<uint16_t>  presentationPort;
        std::vector<char*>     presentationURL;
    } dlnaServer_t;

    typedef struct _srvContent {
        uint16_t size = 0;
        std::vector<char*>     objectId;
        std::vector<char*>     parentId;
        std::vector<uint8_t>   isAudio;
        std::vector<char*>     itemURL;
        std::vector<int32_t>   itemSize;
        std::vector<char*>     duration;
        std::vector<char*>     title;
        std::vector<int16_t>   childCount;
        std::vector<char*>     artist;      // dc:creator, "?" if absent
        std::vector<char*>     album;       // upnp:album, "?" if absent
        std::vector<char*>     albumArtURI; // upnp:albumArtURI, "?" if absent
    } srvContent_t;

    // Callback types, invoked from loop() while the discovery/browse state machine runs.
    typedef void (*InfoCallback)(const char* info);
    typedef void (*ServerFoundCallback)(uint8_t serverId, const char* ip, uint16_t port,
                                         const char* friendlyName, const char* controlURL);
    typedef void (*SeekReadyCallback)(uint8_t numberOfServers);
    typedef void (*BrowseResultCallback)(const char* objectId, const char* parentId, uint16_t childCount,
                                          const char* title, bool isAudio, uint32_t itemSize,
                                          const char* duration, const char* itemURL);
    typedef void (*BrowseReadyCallback)(uint16_t numberReturned, uint16_t totalMatches);

    enum State { IDLE, SEEK_SERVER, GET_SERVER_ITEMS, READ_HTTP_HEADER, BROWSE_SERVER };

    PrioDlnaClient();
    ~PrioDlnaClient();

    void setCallbacks(InfoCallback info = nullptr,
                       ServerFoundCallback serverFound = nullptr,
                       SeekReadyCallback seekReady = nullptr,
                       BrowseResultCallback browseResult = nullptr,
                       BrowseReadyCallback browseReady = nullptr);

    // Sends an SSDP M-SEARCH multicast and starts the discovery state machine.
    // Results arrive asynchronously via loop() -> ServerFoundCallback / SeekReadyCallback.
    bool seekServer();

    // Re-announces already discovered servers via ServerFoundCallback.
    int8_t listServer();

    // Starts a ContentDirectory Browse SOAP request for objectId (use "0" for the root).
    // Results arrive asynchronously via loop() -> BrowseResultCallback / BrowseReadyCallback.
    int8_t browseServer(uint8_t srvNr, const char* objectId,
                         const uint16_t startingIndex = 0, const uint16_t maxCount = 50);

    dlnaServer_t  getServer();
    srvContent_t  getBrowseResult();
    const char*   stringifyServer();
    const char*   stringifyContent();
    uint8_t       getState();
    int16_t       getTotalMatches() { return (m_state == IDLE) ? (int16_t)m_totalMatches : -1; }
    int8_t        getNrOfServers()  { return (m_state == IDLE) ? (int8_t)m_dlnaServer.size : -1; }

    // Must be called regularly (e.g. from a display/network task loop); drives the
    // non-blocking discovery/browse state machine.
    void loop();

private:
    void parseDlnaServer(uint16_t len);
    bool getServerItems(uint8_t srvNr);
    bool browseResult();
    bool srvGet(uint8_t srvNr);
    bool readHttpHeader();
    bool readContent();
    bool srvPost(uint8_t srvNr, const char* objectId, const uint16_t startingIndex, const uint16_t maxCount);

    void vector_clear_and_shrink(std::vector<char*> &vec);
    void dlnaServer_clear_and_shrink();
    void srvContent_clear_and_shrink();

    static int32_t indexOf(const char* haystack, const char* needle, int32_t startIndex);
    static bool    startsWith(const char* base, const char* searchString);
    static bool    endsWith(const char* base, const char* searchString);
    static int     replacestr(char* line, const char* search, const char* replace);
    static char    toLowerAscii(char c);

    char*  x_ps_malloc(uint16_t len);
    char*  x_ps_strdup(const char* str);
    char*  x_ps_strndup(const char* str, uint16_t len);

    dlnaServer_t m_dlnaServer = {};
    srvContent_t m_srvContent = {};

    WiFiClient  m_client;
    WiFiUDP     m_udp;
    State       m_state = IDLE;
    uint32_t    m_timeStamp = 0;
    uint16_t    m_numberReturned = 0;
    uint16_t    m_totalMatches = 0;
    char*       m_JSONstr = nullptr;

    std::vector<char*> m_content;

    InfoCallback         _infoCallback = nullptr;
    ServerFoundCallback  _serverFoundCallback = nullptr;
    SeekReadyCallback    _seekReadyCallback = nullptr;
    BrowseResultCallback _browseResultCallback = nullptr;
    BrowseReadyCallback  _browseReadyCallback = nullptr;

    bool        m_PSRAMfound = false;
    bool        m_chunked = false;
    char*       m_chbuf = nullptr;
    char        m_objectId[60] = {0};
    uint8_t     m_srvNr = 0;
    uint16_t    m_chbufSize = 0;
    uint32_t    m_contentlength = 0;
    uint16_t    m_startingIndex = 0;
    uint16_t    m_maxCount = 100;
};

#endif // PRIO_DLNA_CLIENT_H
