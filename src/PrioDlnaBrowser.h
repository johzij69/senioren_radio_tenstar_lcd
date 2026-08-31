#ifndef PRIO_DLNA_BROWSER_H
#define PRIO_DLNA_BROWSER_H

// Full-screen DLNA server/content browser, opened from PrioRotaryMenu the same
// way AlarmSetup is: a self-contained screen driven by the rotary + button while
// active, that closes itself and hands a chosen track's stream URL and DIDL-Lite
// metadata back via PlayCallback (wire this to Task_Display's playDlnaTrack(),
// which forwards to AudioControl's playAudio() and pushes the metadata to the
// display queue).
//
// PrioDlnaClient's network I/O is blocking, so it runs on its own DlnaTask
// (see Task_Dlna.h/.cpp) - this class only ever reads results back via
// PrioDlnaClient::getServer()/getBrowseResult() and sends commands/receives
// events through the two queues declared there, so DisplayTask never blocks.

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <vector>
#include "PrioDlnaClient.h"
#include "Task_Dlna.h"

class PrioDlnaBrowser {
public:
    typedef void (*PlayCallback)(const char* url, const char* title, const char* artist,
                                  const char* album, const char* albumArtURI);
    // Called when the browser closes without a track having been chosen (cancelled
    // while searching/browsing, "no servers found", or "... Terug" from the server
    // list) - wire this to something that resumes whatever was playing before the
    // browser stopped it (see start()), e.g. the last-played favorite.
    typedef void (*CancelledCallback)();

    PrioDlnaBrowser(TFT_eSPI &tft, PrioDlnaClient &dlnaClient, PlayCallback playCallback,
                     CancelledCallback cancelledCallback);

    void start();
    void stop();
    bool isActive() const { return _active; }

    void onButtonPress();
    void onRotaryDelta(int delta);
    void loop();

    // Called by DisplayTask when the currently playing DLNA track reaches its
    // natural end (Task_audio's audio_eof_stream -> AudioEventQueue), so the
    // player keeps going: advances to the next audio item from the folder the
    // current track was chosen from, wrapping back to the first after the
    // last. No-op if nothing was ever played from here (e.g. a preset/alarm
    // stream is currently playing instead).
    void playNext();

private:
    enum Screen { SCREEN_STATUS, SCREEN_SERVERS, SCREEN_BROWSE };

    void sendCommand(const DlnaCommand &cmd);
    // Closes the browser WITHOUT a track chosen - invokes CancelledCallback (see
    // start()'s stopAudio()) so playback resumes, then stop(). Kept distinct from
    // the "track chosen" path in onButtonPress(), which calls stop() directly since
    // _playCallback already started new playback there.
    void cancel();
    void handleSeekReady(uint8_t numberOfServers);
    void handleBrowseReady(uint16_t numberReturned, uint16_t totalMatches);

    void enterServerList();
    void enterBrowse(const char* objectId);
    void showStatus(const String &text);
    void rescanServers(); // "... Vernieuwen": forces a fresh SSDP scan, bypassing the server cache

    int itemCount() const;
    // Verschuift het venster per hele pagina i.p.v. per regel; true als de pagina wisselde.
    bool updateScrollOffset(int total);
    String truncateToFit(const char* text, int maxWidth);

    void draw();
    void drawHeader(const char* title);
    void drawFooter(const char* text);
    void drawStatusScreen();
    void drawServerList(bool fullRedraw);
    void drawBrowseList(bool fullRedraw);
    void drawListItem(int row, const char* label, bool selected);

    TFT_eSPI* _tft;
    PrioDlnaClient* _dlna;
    PlayCallback _playCallback;
    CancelledCallback _cancelledCallback;

    bool _active = false;
    bool _needsRedraw = true;
    bool _busy = false; // waiting for seekServer()/browseServer() to complete
    bool _firstDraw = true;
    Screen _screen = SCREEN_STATUS;
    Screen _prevScreen = SCREEN_STATUS;

    String _statusText;
    int _selectedIndex = 0;
    int _prevSelectedIndex = -1;
    int _scrollOffset = 0;

    int8_t _currentServerIndex = -1;
    std::vector<String> _folderStack; // parent objectIds, for "... Terug" navigation
    String _currentObjectId = "0";

    // Auto-advance playlist: every audio item from the folder the most
    // recently played track came from, snapshotted at play time (see
    // onButtonPress()) so it stays valid even after the browser closes.
    std::vector<String> _playlistUrl;
    std::vector<String> _playlistTitle;
    std::vector<String> _playlistArtist;
    std::vector<String> _playlistAlbum;
    std::vector<String> _playlistAlbumArt;
    int _playlistIndex = -1;

    static constexpr int LIST_START_Y = 50;
    static constexpr int ITEM_HEIGHT = 40;
    static constexpr int VISIBLE_ROWS = 6;

    // Same look and feel as AlarmSetup / PrioRotaryMenu.
    static constexpr uint16_t BG_COLOR = TFT_BLACK;
    static constexpr uint16_t FG_COLOR = TFT_WHITE;
    static constexpr uint16_t HIGHLIGHT_COLOR = TFT_BLUE;
    static constexpr uint16_t HEADER_BG = TFT_NAVY;
    static constexpr uint16_t HEADER_FG = TFT_WHITE;
};

#endif // PRIO_DLNA_BROWSER_H
