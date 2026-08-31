#include "PrioDlnaBrowser.h"
#include "AudioControl.h"
#include <string.h>

extern QueueHandle_t DlnaCommandQueue;
extern QueueHandle_t DlnaEventQueue;
// Set after every fillScreen() so Task_Display.cpp's rotary-button read ignores
// the EMI blip a big SPI transfer can induce on the button GPIO (same guard the
// rest of the UI uses around its own redraws).
extern unsigned long redrawLockoutTime;

PrioDlnaBrowser::PrioDlnaBrowser(TFT_eSPI &tft, PrioDlnaClient &dlnaClient, PlayCallback playCallback,
                                 CancelledCallback cancelledCallback)
    : _tft(&tft), _dlna(&dlnaClient), _playCallback(playCallback), _cancelledCallback(cancelledCallback) {
}

void PrioDlnaBrowser::sendCommand(const DlnaCommand &cmd) {
    xQueueSend(DlnaCommandQueue, &cmd, 0);
}

//------------------------------------------------------------------------------------------------
void PrioDlnaBrowser::start() {
    // SSDP discovery and the SOAP browse requests are blocking network I/O on
    // DlnaTask; sharing the WiFi radio/CPU with a decoding audio stream made
    // it audibly stutter, so stop playback for the whole time the browser is
    // open (search and folder browsing alike) and resume it in the cancel
    // paths below if the user backs out without choosing a track.
    stopAudio();

    _active = true;
    _screen = SCREEN_STATUS;
    _prevScreen = SCREEN_STATUS;
    _firstDraw = true;
    _needsRedraw = true;
    _busy = true;
    _selectedIndex = 0;
    _prevSelectedIndex = -1;
    _scrollOffset = 0;
    _currentServerIndex = -1;
    _folderStack.clear();
    _currentObjectId = "0";
    _statusText = "Zoeken naar DLNA-servers...";

    // Drop any stale event left over from a session the user cancelled before
    // DlnaTask got around to reporting it, so it isn't misread as this seek's result.
    xQueueReset(DlnaEventQueue);

    DlnaCommand cmd;
    cmd.command = DLNA_CMD_SEEK;
    cmd.serverIndex = 0;
    cmd.objectId[0] = '\0';
    cmd.forceRescan = false; // use the cached server list if there is one
    sendCommand(cmd);
}

void PrioDlnaBrowser::stop() {
    _active = false;
    _needsRedraw = false;
}

void PrioDlnaBrowser::cancel() {
    if (_cancelledCallback) _cancelledCallback();
    stop();
}

//------------------------------------------------------------------------------------------------
void PrioDlnaBrowser::onButtonPress() {
    if (!_active) return;

    if (_busy) { cancel(); return; } // cancel while searching/loading

    if (_screen == SCREEN_STATUS) { cancel(); return; } // message screen (e.g. "no servers found")

    if (_screen == SCREEN_SERVERS) {
        if (_selectedIndex == 0) { cancel(); return; } // "... Terug" -> close browser
        if (_selectedIndex == 1) { rescanServers(); return; } // "... Vernieuwen"
        PrioDlnaClient::dlnaServer_t servers = _dlna->getServer();
        int srvIdx = _selectedIndex - 2;
        if (srvIdx < 0 || srvIdx >= servers.size) return;
        _currentServerIndex = (int8_t)srvIdx;
        _folderStack.clear();
        _currentObjectId = "0";
        enterBrowse(_currentObjectId.c_str());
        return;
    }

    if (_screen == SCREEN_BROWSE) {
        if (_selectedIndex == 0) { // "... Terug": up one folder, or back to the server list at root
            if (_folderStack.empty()) {
                enterServerList();
            } else {
                _currentObjectId = _folderStack.back();
                _folderStack.pop_back();
                enterBrowse(_currentObjectId.c_str());
            }
            return;
        }

        PrioDlnaClient::srvContent_t content = _dlna->getBrowseResult();
        int idx = _selectedIndex - 1;
        if (idx < 0 || idx >= content.size) return;

        bool isContainer = (strcmp(content.itemURL[idx], "?") == 0);
        if (isContainer) {
            _folderStack.push_back(_currentObjectId);
            _currentObjectId = String(content.objectId[idx]);
            enterBrowse(_currentObjectId.c_str());
        } else if (strcmp(content.itemURL[idx], "?") != 0) {
            // Snapshot every playable item in this folder as the auto-advance
            // playlist, so a natural end-of-track can continue with the next
            // one (see playNext()) even after this browser screen closes.
            _playlistUrl.clear(); _playlistTitle.clear(); _playlistArtist.clear();
            _playlistAlbum.clear(); _playlistAlbumArt.clear();
            _playlistIndex = -1;
            for (int i = 0; i < content.size; i++) {
                if (strcmp(content.itemURL[i], "?") == 0) continue; // folder, not a track
                if (i == idx) _playlistIndex = (int)_playlistUrl.size();
                _playlistUrl.push_back(content.itemURL[i]);
                _playlistTitle.push_back(content.title[i]);
                _playlistArtist.push_back(content.artist[i]);
                _playlistAlbum.push_back(content.album[i]);
                _playlistAlbumArt.push_back(content.albumArtURI[i]);
            }

            if (_playCallback) {
                _playCallback(content.itemURL[idx], content.title[idx], content.artist[idx],
                              content.album[idx], content.albumArtURI[idx]);
            }
            stop(); // hand off to the player and return to the normal player screen
        }
    }
}

void PrioDlnaBrowser::onRotaryDelta(int delta) {
    if (!_active || _busy) return;
    int count = itemCount();
    if (count <= 0) return;
    _selectedIndex += delta;
    if (_selectedIndex < 0) _selectedIndex = count - 1;
    if (_selectedIndex >= count) _selectedIndex = 0;
    _needsRedraw = true;
}

void PrioDlnaBrowser::loop() {
    if (!_active) return;

    DlnaEvent evt;
    while (xQueueReceive(DlnaEventQueue, &evt, 0) == pdTRUE) {
        switch (evt.type) {
            case DLNA_EVT_SEEK_READY:
                handleSeekReady((uint8_t)evt.count);
                break;
            case DLNA_EVT_SEEK_FAILED:
                _busy = false;
                showStatus("Geen WiFi-verbinding");
                break;
            case DLNA_EVT_BROWSE_READY:
                handleBrowseReady(evt.count, evt.count);
                break;
            case DLNA_EVT_BROWSE_FAILED:
                _busy = false;
                showStatus("Kan map niet laden");
                break;
        }
    }

    if (_needsRedraw) {
        draw();
        _needsRedraw = false;
    }
}

void PrioDlnaBrowser::playNext() {
    if (_playlistUrl.empty() || !_playCallback) return;
    _playlistIndex = (_playlistIndex + 1) % (int)_playlistUrl.size();
    _playCallback(_playlistUrl[_playlistIndex].c_str(), _playlistTitle[_playlistIndex].c_str(),
                  _playlistArtist[_playlistIndex].c_str(), _playlistAlbum[_playlistIndex].c_str(),
                  _playlistAlbumArt[_playlistIndex].c_str());
}

//------------------------------------------------------------------------------------------------
int PrioDlnaBrowser::itemCount() const {
    if (_screen == SCREEN_SERVERS) return 2 + _dlna->getServer().size; // "... Terug" + "... Vernieuwen"
    if (_screen == SCREEN_BROWSE)  return 1 + _dlna->getBrowseResult().size;
    return 0;
}

bool PrioDlnaBrowser::updateScrollOffset(int total) {
    int oldOffset = _scrollOffset;

    if (_selectedIndex >= _scrollOffset + VISIBLE_ROWS) {
        // Vooruit voorbij de onderste regel: volgende pagina, selectie bovenaan.
        _scrollOffset = _selectedIndex;
    } else if (_selectedIndex < _scrollOffset) {
        // Terug voorbij de bovenste regel: vorige pagina, selectie onderaan.
        _scrollOffset = _selectedIndex - VISIBLE_ROWS + 1;
    }

    if (_scrollOffset > total - 1) _scrollOffset = total - 1;
    if (_scrollOffset < 0) _scrollOffset = 0;

    return oldOffset != _scrollOffset;
}

void PrioDlnaBrowser::enterServerList() {
    _screen = SCREEN_SERVERS;
    _selectedIndex = 0;
    _prevSelectedIndex = -1;
    _scrollOffset = 0;
    _needsRedraw = true;
}

void PrioDlnaBrowser::enterBrowse(const char* objectId) {
    if (_currentServerIndex < 0) { enterServerList(); return; }
    _busy = true;
    showStatus("Laden...");

    DlnaCommand cmd;
    cmd.command = DLNA_CMD_BROWSE;
    cmd.serverIndex = (uint8_t)_currentServerIndex;
    strncpy(cmd.objectId, objectId, sizeof(cmd.objectId) - 1);
    cmd.objectId[sizeof(cmd.objectId) - 1] = '\0';
    sendCommand(cmd);
}

void PrioDlnaBrowser::showStatus(const String &text) {
    _statusText = text;
    _screen = SCREEN_STATUS;
    _needsRedraw = true;
}

void PrioDlnaBrowser::rescanServers() {
    _busy = true;
    showStatus("Zoeken naar DLNA-servers...");

    xQueueReset(DlnaEventQueue);

    DlnaCommand cmd;
    cmd.command = DLNA_CMD_SEEK;
    cmd.serverIndex = 0;
    cmd.objectId[0] = '\0';
    cmd.forceRescan = true;
    sendCommand(cmd);
}

//------------------------------------------------------------------------------------------------
void PrioDlnaBrowser::handleSeekReady(uint8_t numberOfServers) {
    if (!_active) return; // browser was closed while the search was still running
    _busy = false;
    if (numberOfServers == 0) {
        showStatus("Geen DLNA-servers gevonden");
        return;
    }
    enterServerList();
}

void PrioDlnaBrowser::handleBrowseReady(uint16_t numberReturned, uint16_t totalMatches) {
    (void)numberReturned;
    (void)totalMatches;
    if (!_active) return;
    _busy = false;
    _screen = SCREEN_BROWSE;
    _selectedIndex = 0;
    _prevSelectedIndex = -1;
    _scrollOffset = 0;
    _needsRedraw = true;
}

//------------------------------------------------------------------------------------------------
String PrioDlnaBrowser::truncateToFit(const char* text, int maxWidth) {
    String s(text);
    if (_tft->textWidth(s) <= maxWidth) return s;
    while (s.length() > 1 && _tft->textWidth(s + "..") > maxWidth) {
        s.remove(s.length() - 1);
    }
    return s + "..";
}

void PrioDlnaBrowser::draw() {
    bool screenChanged = _firstDraw || (_screen != _prevScreen);
    if (screenChanged) {
        _tft->fillScreen(BG_COLOR);
        _firstDraw = false;
        _prevScreen = _screen;
        redrawLockoutTime = millis();
    }
    switch (_screen) {
        case SCREEN_STATUS:  drawStatusScreen(); break;
        case SCREEN_SERVERS: drawServerList(screenChanged); break;
        case SCREEN_BROWSE:  drawBrowseList(screenChanged); break;
    }
    _prevSelectedIndex = _selectedIndex;
}

void PrioDlnaBrowser::drawHeader(const char* title) {
    _tft->fillRect(0, 0, 480, 40, HEADER_BG);
    _tft->setTextFont(1);
    _tft->setTextSize(1);
    _tft->setTextColor(HEADER_FG, HEADER_BG);
    _tft->setTextDatum(MC_DATUM);
    _tft->drawString(title, 240, 20);
}

void PrioDlnaBrowser::drawFooter(const char* text) {
    _tft->setTextFont(1);
    _tft->setTextSize(1);
    _tft->setTextColor(TFT_LIGHTGREY, BG_COLOR);
    _tft->setTextDatum(BC_DATUM);
    _tft->drawString(text, 240, 315);
}

void PrioDlnaBrowser::drawStatusScreen() {
    drawHeader("DLNA");
    _tft->setTextFont(1);
    _tft->setTextSize(2);
    _tft->setTextColor(FG_COLOR, BG_COLOR);
    _tft->setTextDatum(MC_DATUM);
    _tft->drawString(_statusText, 240, 160);
    drawFooter(_busy ? "Druk = annuleren" : "Druk = terug");
}

void PrioDlnaBrowser::drawServerList(bool fullRedraw) {
    PrioDlnaClient::dlnaServer_t servers = _dlna->getServer();
    int total = 2 + servers.size;

    bool scrollChanged = updateScrollOffset(total);

    auto labelFor = [&](int idx) -> const char* {
        if (idx == 0) return "... Terug";
        if (idx == 1) return "... Vernieuwen";
        return servers.friendlyName[idx - 2];
    };

    if (fullRedraw) drawHeader("DLNA Servers");

    if (fullRedraw || scrollChanged) {
        _tft->fillRect(0, LIST_START_Y, 480, VISIBLE_ROWS * ITEM_HEIGHT, BG_COLOR);
        for (int row = 0; row < VISIBLE_ROWS; row++) {
            int idx = _scrollOffset + row;
            if (idx >= total) break;
            drawListItem(row, labelFor(idx), idx == _selectedIndex);
        }
    } else if (_prevSelectedIndex != _selectedIndex) {
        int prevRow = _prevSelectedIndex - _scrollOffset;
        int curRow  = _selectedIndex - _scrollOffset;
        if (prevRow >= 0 && prevRow < VISIBLE_ROWS) drawListItem(prevRow, labelFor(_prevSelectedIndex), false);
        if (curRow  >= 0 && curRow  < VISIBLE_ROWS) drawListItem(curRow,  labelFor(_selectedIndex), true);
    }

    if (fullRedraw) drawFooter("Draai = selecteer  |  Druk = open/kies");
}

void PrioDlnaBrowser::drawBrowseList(bool fullRedraw) {
    PrioDlnaClient::srvContent_t content = _dlna->getBrowseResult();
    int total = 1 + content.size;

    bool scrollChanged = updateScrollOffset(total);

    if (fullRedraw) {
        PrioDlnaClient::dlnaServer_t servers = _dlna->getServer();
        const char* serverName = (_currentServerIndex >= 0 && _currentServerIndex < servers.size)
                                      ? servers.friendlyName[_currentServerIndex]
                                      : "DLNA";
        drawHeader(serverName);
    }

    auto labelFor = [&](int idx) -> String {
        if (idx == 0) return "... Terug";
        int i = idx - 1;
        bool isContainer = (strcmp(content.itemURL[i], "?") == 0);
        String prefix = isContainer ? "[Map] " : "";
        return prefix + String(content.title[i]);
    };

    if (fullRedraw || scrollChanged) {
        _tft->fillRect(0, LIST_START_Y, 480, VISIBLE_ROWS * ITEM_HEIGHT, BG_COLOR);
        for (int row = 0; row < VISIBLE_ROWS; row++) {
            int idx = _scrollOffset + row;
            if (idx >= total) break;
            drawListItem(row, labelFor(idx).c_str(), idx == _selectedIndex);
        }
    } else if (_prevSelectedIndex != _selectedIndex) {
        int prevRow = _prevSelectedIndex - _scrollOffset;
        int curRow  = _selectedIndex - _scrollOffset;
        if (prevRow >= 0 && prevRow < VISIBLE_ROWS) drawListItem(prevRow, labelFor(_prevSelectedIndex).c_str(), false);
        if (curRow  >= 0 && curRow  < VISIBLE_ROWS) drawListItem(curRow,  labelFor(_selectedIndex).c_str(), true);
    }

    if (fullRedraw) drawFooter("Draai = selecteer  |  Druk = open/afspelen");
}

void PrioDlnaBrowser::drawListItem(int row, const char* label, bool selected) {
    int y = LIST_START_Y + row * ITEM_HEIGHT;
    _tft->fillRect(10, y + 2, 460, ITEM_HEIGHT - 4, selected ? HIGHLIGHT_COLOR : BG_COLOR);
    _tft->setTextFont(1);
    _tft->setTextSize(2);
    _tft->setTextDatum(ML_DATUM);
    _tft->setTextColor(FG_COLOR, selected ? HIGHLIGHT_COLOR : BG_COLOR);
    String text = truncateToFit(label, 440);
    _tft->drawString(text, 20, y + ITEM_HEIGHT / 2);
}
