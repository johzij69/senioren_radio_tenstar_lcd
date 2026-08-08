#include "PrioRotaryMenu.h"

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
    if (!_isOpen) {
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
    if (!_isOpen) {
        return;
    }

    _menuDelta += delta;
    _needsRedraw = true;
}

bool PrioRotaryMenu::isOpen() {
    return _isOpen;
}

void PrioRotaryMenu::loop() {
    if (!_isOpen) {
        return;
    }

    if (_needsRedraw) {
        updateSelection();
        drawMenu();
        _needsRedraw = false;
    }
}

void PrioRotaryMenu::handleSelection() {
    if (_state == MAIN_MENU) {

        JsonArray categories = _menuDoc.as<JsonArray>();

        if (_selectedMainIndex == categories.size()) {

            _isOpen = false;
            _needsRedraw = false;
            _stateChanged = true;
            _state = MAIN_MENU;
            _selectedMainIndex = 0;

            if (_closeCallback) {
                _closeCallback();
            }

        } else {

            _currentCategoryIndex = _selectedMainIndex;
            _state = SUB_MENU;
            _selectedSubIndex = 0;
            _stateChanged = true;
            _needsRedraw = true;
        }

    } else if (_state == SUB_MENU) {

        JsonArray categories = _menuDoc.as<JsonArray>();
        JsonObject category = categories[_currentCategoryIndex];
        JsonArray items = category["items"];

        if (_selectedSubIndex == 0) {

            _state = MAIN_MENU;
            _stateChanged = true;
            _needsRedraw = true;

        } else {

            JsonObject item = items[_selectedSubIndex - 1];
            const char* action = item["action"];

            if (_actionCallback && action) {
                _actionCallback(action);
            }
        }
    }
}

void PrioRotaryMenu::updateSelection() {
    if (_menuDelta == 0) {
        return;
    }

    if (_state == MAIN_MENU) {

        JsonArray categories = _menuDoc.as<JsonArray>();
        int maxIdx = categories.size();

        _selectedMainIndex += _menuDelta;

        if (_selectedMainIndex < 0) {
            _selectedMainIndex = maxIdx;
        }

        if (_selectedMainIndex > maxIdx) {
            _selectedMainIndex = 0;
        }

    } else if (_state == SUB_MENU) {

        JsonArray categories = _menuDoc.as<JsonArray>();
        JsonObject category = categories[_currentCategoryIndex];
        JsonArray items = category["items"];

        int totalItems = items.size() + 1;

        _selectedSubIndex += _menuDelta;

        if (_selectedSubIndex < 0) {
            _selectedSubIndex = totalItems - 1;
        }

        if (_selectedSubIndex >= totalItems) {
            _selectedSubIndex = 0;
        }
    }

    _menuDelta = 0;
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

        _stateChanged = false;
    }

    if (_state == MAIN_MENU) {
        drawMainMenuItems();
    } else if (_state == SUB_MENU) {
        drawSubMenuItems();
    }
}

void PrioRotaryMenu::drawHeader(const char* title) {
    _tft->fillRect(0, 0, 320, 50, MENU_HEADER_BG);

    _tft->setTextColor(MENU_HEADER_FG, MENU_HEADER_BG);
    _tft->setTextDatum(MC_DATUM);
    _tft->setTextSize(2);

    _tft->drawString(title, 160, 25);
}

void PrioRotaryMenu::drawMainMenuItems() {
    JsonArray categories = _menuDoc.as<JsonArray>();

    int startY = 50;
    int itemHeight = 45;
    int maxItems = (480 - startY) / itemHeight;

    _tft->setTextSize(2);
    _tft->setTextDatum(ML_DATUM);

    int count = 0;

    for (JsonObject category : categories) {

        if (count >= maxItems) {
            break;
        }

        const char* label = category["label"];

        int y = startY + count * itemHeight;
        bool isSelected = (count == _selectedMainIndex);

        if (isSelected) {

            _tft->fillRect(10, y + 5, 300, itemHeight - 10, MENU_HIGHLIGHT);
            _tft->setTextColor(TFT_WHITE, MENU_HIGHLIGHT);

        } else {

            _tft->fillRect(10, y + 5, 300, itemHeight - 10, MENU_BG_COLOR);
            _tft->setTextColor(MENU_FG_COLOR, MENU_BG_COLOR);
        }

        _tft->drawString(label, 20, y + itemHeight / 2);

        count++;
    }

    if (count < maxItems) {

        int y = startY + count * itemHeight;
        bool isSelected = (count == _selectedMainIndex);

        if (isSelected) {

            _tft->fillRect(10, y + 5, 300, itemHeight - 10, TFT_DARKGREY);
            _tft->setTextColor(TFT_WHITE, TFT_DARKGREY);

        } else {

            _tft->fillRect(10, y + 5, 300, itemHeight - 10, MENU_BG_COLOR);
            _tft->setTextColor(TFT_LIGHTGREY, MENU_BG_COLOR);
        }

        _tft->drawString("Close Menu", 20, y + itemHeight / 2);
    }
}

void PrioRotaryMenu::drawSubMenuItems() {
    JsonArray categories = _menuDoc.as<JsonArray>();
    JsonObject category = categories[_currentCategoryIndex];
    JsonArray items = category["items"];

    int startY = 50;
    int itemHeight = 45;
    int maxItems = (480 - startY) / itemHeight;

    _tft->setTextSize(2);
    _tft->setTextDatum(ML_DATUM);

    int count = 0;

    if (count < maxItems) {

        int y = startY + count * itemHeight;
        bool isSelected = (count == _selectedSubIndex);

        if (isSelected) {

            _tft->fillRect(10, y + 5, 300, itemHeight - 10, TFT_DARKGREY);
            _tft->setTextColor(TFT_WHITE, TFT_DARKGREY);

        } else {

            _tft->fillRect(10, y + 5, 300, itemHeight - 10, MENU_BG_COLOR);
            _tft->setTextColor(TFT_LIGHTGREY, MENU_BG_COLOR);
        }

        _tft->drawString("< Back", 20, y + itemHeight / 2);

        count++;
    }

    for (JsonObject item : items) {

        if (count >= maxItems) {
            break;
        }

        const char* label = item["label"];

        int y = startY + count * itemHeight;
        bool isSelected = (count == _selectedSubIndex);

        if (isSelected) {

            _tft->fillRect(10, y + 5, 300, itemHeight - 10, MENU_HIGHLIGHT);
            _tft->setTextColor(TFT_WHITE, MENU_HIGHLIGHT);

        } else {

            _tft->fillRect(10, y + 5, 300, itemHeight - 10, MENU_BG_COLOR);
            _tft->setTextColor(MENU_FG_COLOR, MENU_BG_COLOR);
        }

        _tft->drawString(label, 20, y + itemHeight / 2);

        count++;
    }
}