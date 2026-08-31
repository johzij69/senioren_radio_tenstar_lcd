#include "PrioRotaryMenu.h"
#include "generalHelpers.h"

PrioRotaryMenu::PrioRotaryMenu(TFT_eSPI &tft) {
    _tft = &tft;
}

void PrioRotaryMenu::setCallbacks(ActionCallback action,
                              MenuStateCallback open,
                              MenuStateCallback close) {
    _actionCallback = action;
    _openCallback = open;
    _closeCallback = close;
}

bool PrioRotaryMenu::loadMenu(const char* jsonString) {
    DeserializationError error = deserializeJson(_menuDoc, jsonString);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return false;
    }
    _stateChanged = true;
    return true;
}

void PrioRotaryMenu::onButtonPress() {
    unsigned long currentTime = millis();
    if (currentTime - _lastActionTime < 300) {
        return;
    }
    _lastActionTime = currentTime;

    if (!_isOpen) {
        saveTextStyle();
        _isOpen = true;
        _needsRedraw = true;
        _stateChanged = true;
        if (_openCallback) {
            _openCallback();
        }
    } else {
        handleSelection();
    }
}

void PrioRotaryMenu::onRotaryDelta(int delta) {
    if (!_isOpen) return;
    _menuDelta += delta;
    _needsRedraw = true;
}

bool PrioRotaryMenu::isOpen() {
    return _isOpen;
}

void PrioRotaryMenu::loop() {
    if (!_isOpen) return;
    if (_needsRedraw) {
        updateSelection();
        drawMenu();
        _needsRedraw = false;
    }
}

void PrioRotaryMenu::handleSelection() {
    if (_state == MAIN_MENU) {
        JsonArray categories = _menuDoc.as<JsonArray>();
        if (_selectedMainIndex == 0) {
            closeMenu();
        } else if (_selectedMainIndex > 0 && _selectedMainIndex <= (int)categories.size()) {
            _currentCategoryIndex = _selectedMainIndex - 1;
            _state = SUB_MENU;
            _selectedSubIndex = 0;
            _scrollOffset = 0;
            _stateChanged = true;
            _needsRedraw = true;
        }
    } else if (_state == SUB_MENU) {
        if (_selectedSubIndex == 0) {
            _state = MAIN_MENU;
            _scrollOffset = 0;
            _stateChanged = true;
            _needsRedraw = true;
        } else {
            JsonArray categories = _menuDoc.as<JsonArray>();
            JsonObject category = categories[_currentCategoryIndex];
            JsonArray items = category["items"];
            int itemIdx = _selectedSubIndex - 1;
            if (itemIdx >= 0 && itemIdx < (int)items.size()) {
                JsonObject item = items[itemIdx];
                const char* action = item["action"];
                if (_actionCallback && action) {
                    _actionCallback(action);
                    // The callback may have closed the menu itself (e.g. to hand off to
                    // a full-screen mode like AlarmSetup/PrioDlnaBrowser); don't revive
                    // its redraw flags in that case, or it repaints after being closed.
                    if (_isOpen) {
                        _needsRedraw = true;
                        _stateChanged = true;
                    }
                }
            }
        }
    }
}

void PrioRotaryMenu::updateSelection() {
    if (_menuDelta == 0) return;

    if (_state == MAIN_MENU) {
        JsonArray categories = _menuDoc.as<JsonArray>();
        int maxIdx = categories.size() + 1;
        _selectedMainIndex += _menuDelta;
        if (_selectedMainIndex < 0) _selectedMainIndex = maxIdx - 1;
        if (_selectedMainIndex >= maxIdx) _selectedMainIndex = 0;
    } else if (_state == SUB_MENU) {
        JsonArray categories = _menuDoc.as<JsonArray>();
        JsonObject category = categories[_currentCategoryIndex];
        JsonArray items = category["items"];
        int totalItems = items.size() + 1;
        _selectedSubIndex += _menuDelta;
        if (_selectedSubIndex < 0) _selectedSubIndex = totalItems - 1;
        if (_selectedSubIndex >= totalItems) _selectedSubIndex = 0;
    }
    _menuDelta = 0;
}

void PrioRotaryMenu::closeMenu() {
    if (!_isOpen) return;
    _isOpen = false;
    _needsRedraw = false;
    _stateChanged = true;
    _state = MAIN_MENU;
    _selectedMainIndex = 0;
    _selectedSubIndex = 0;
    _currentCategoryIndex = 0;
    _scrollOffset = 0;
    _menuDelta = 0;
    restoreTextStyle();
    if (_closeCallback) {
        _closeCallback();
    }
}

void PrioRotaryMenu::drawMenu() {
    if (_stateChanged) {
        _tft->fillScreen(MENU_BG_COLOR);
        if (_state == MAIN_MENU) {
            drawHeader("Main Menu");
        } else if (_state == SUB_MENU) {
            JsonArray categories = _menuDoc.as<JsonArray>();
            JsonObject category = categories[_currentCategoryIndex];
            drawHeader(category["label"]);
        }
        drawFooter();
        _stateChanged = false;
    }
    if (_state == MAIN_MENU) {
        drawMainMenuItems();
    } else if (_state == SUB_MENU) {
        drawSubMenuItems();
    }
}

void PrioRotaryMenu::drawHeader(const char* title) {
    _tft->fillRect(0, 0, 480, 40, MENU_HEADER_BG);
    _tft->setTextFont(1);
    _tft->setTextSize(1);
    _tft->setTextColor(MENU_HEADER_FG, MENU_HEADER_BG);
    _tft->setTextDatum(MC_DATUM);
    _tft->drawString(title, 240, 20);
}

void PrioRotaryMenu::drawFooter() {
    _tft->setTextFont(1);
    _tft->setTextSize(1);
    _tft->setTextColor(TFT_LIGHTGREY, MENU_BG_COLOR);
    _tft->setTextDatum(BC_DATUM);
    _tft->drawString("Draai = selecteer  |  Druk = open/kies", 240, 315);
}

void PrioRotaryMenu::drawListWindow(int selectedIndex, int total, const std::function<const char*(int)> &labelFor) {
    updatePageOffset(selectedIndex, total, VISIBLE_ROWS, _scrollOffset);

    for (int row = 0; row < VISIBLE_ROWS; row++) {
        int idx = _scrollOffset + row;
        int y = LIST_START_Y + row * ITEM_HEIGHT;
        if (idx >= total) {
            _tft->fillRect(10, y + 2, 460, ITEM_HEIGHT - 4, MENU_BG_COLOR);
            continue;
        }
        drawMenuItem(y, ITEM_HEIGHT, labelFor(idx), idx == selectedIndex);
    }
}

void PrioRotaryMenu::drawMainMenuItems() {
    JsonArray categories = _menuDoc.as<JsonArray>();
    int total = 1 + (int)categories.size();

    drawListWindow(_selectedMainIndex, total, [&](int idx) -> const char* {
        if (idx == 0) return "... Terug";
        return categories[idx - 1]["label"].as<const char*>();
    });
}

void PrioRotaryMenu::drawSubMenuItems() {
    JsonArray categories = _menuDoc.as<JsonArray>();
    JsonObject category = categories[_currentCategoryIndex];
    JsonArray items = category["items"];
    int total = 1 + (int)items.size();

    drawListWindow(_selectedSubIndex, total, [&](int idx) -> const char* {
        if (idx == 0) return "... Terug";
        return items[idx - 1]["label"].as<const char*>();
    });
}

void PrioRotaryMenu::drawMenuItem(int y, int itemHeight, const char* label, bool selected) {
    _tft->fillRect(10, y + 2, 460, itemHeight - 4, selected ? MENU_HIGHLIGHT : MENU_BG_COLOR);
    _tft->setTextFont(1);
    _tft->setTextSize(2);
    _tft->setTextDatum(ML_DATUM);
    _tft->setTextColor(selected ? TFT_WHITE : MENU_FG_COLOR, selected ? MENU_HIGHLIGHT : MENU_BG_COLOR);
    _tft->drawString(label, 20, y + itemHeight / 2);
}

void PrioRotaryMenu::saveTextStyle() {
    _savedTextStyle.textSize  = _tft->textsize;
    _savedTextStyle.textDatum = _tft->textdatum;
    _savedTextStyle.textFont  = _tft->textfont;
    _savedTextStyle.fgColor   = _tft->textcolor;
    _savedTextStyle.bgColor   = _tft->textbgcolor;
    _savedTextStyle.cursorX   = _tft->getCursorX();
    _savedTextStyle.cursorY   = _tft->getCursorY();
    _savedTextStyle.valid     = true;
}

void PrioRotaryMenu::restoreTextStyle() {
    if (!_savedTextStyle.valid) return;
    _tft->setTextFont(_savedTextStyle.textFont);
    _tft->setTextSize(_savedTextStyle.textSize);
    _tft->setTextDatum(_savedTextStyle.textDatum);
    _tft->setTextColor(_savedTextStyle.fgColor, _savedTextStyle.bgColor);
    _tft->setCursor(_savedTextStyle.cursorX, _savedTextStyle.cursorY);
    _savedTextStyle.valid = false;
}