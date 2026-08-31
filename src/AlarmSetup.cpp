#include "AlarmSetup.h"
#include "generalHelpers.h"

static String alarmModeToString(AlarmManager::RepeatMode mode) {
    switch (mode) {
        case AlarmManager::REPEAT_WEEKDAYS: return "weekdays";
        case AlarmManager::REPEAT_WEEKEND:  return "weekend";
        case AlarmManager::REPEAT_WEEKLY:   return "weekly";
        case AlarmManager::REPEAT_CUSTOM:   return "custom";
        default: return "daily";
    }
}

AlarmSetup::AlarmSetup(TFT_eSPI &tft, AlarmManager &alarmMgr, UrlManager &urlMgr) 
    : _tft(&tft), _alarmMgr(&alarmMgr), _urlMgr(&urlMgr) {}

void AlarmSetup::start() {
    _active = true;
    _screen = SCREEN_LIST;
    _prevScreen = SCREEN_LIST;
    _listSelection = 0;
    _prevListSelection = -1;
    _scrollOffset = 0;
    _selectedAlarmIndex = -1;
    _selectedField = 0;
    _prevField = -1;
    _editMode = false;
    _prevEditMode = false;
    _needsRedraw = true;
    _firstDraw = true;
    _waitForBtnRelease = true;  // Wacht tot knop los is
    
    Serial.println("AlarmSetup: gestart, wacht op knop-release");
}

void AlarmSetup::stop() {
    _active = false;
    _needsRedraw = false;
    Serial.println("AlarmSetup: gestopt");
}

void AlarmSetup::onRotaryDelta(int delta) {
    if (!_active) return;
    
    if (_screen == SCREEN_LIST) {
        int maxIdx = _alarmMgr->getCount() + 1; // 0=terug, 1..count=alarmen, count+1=nieuw
        _listSelection += delta;
        if (_listSelection < 0) _listSelection = maxIdx;
        if (_listSelection > maxIdx) _listSelection = 0;
    }
    else if (_screen == SCREEN_EDIT) {
        if (_editMode) {
            switch (_selectedField) {
                case FIELD_HOUR:
                    _editAlarm.hour = (_editAlarm.hour + delta + 24) % 24;
                    break;
                case FIELD_MINUTE:
                    _editAlarm.minute = (_editAlarm.minute + delta + 60) % 60;
                    break;
                case FIELD_STREAM:
                    if (_urlMgr->streamCount > 0) {
                        _editAlarm.streamIndex = (_editAlarm.streamIndex + delta + _urlMgr->streamCount) % _urlMgr->streamCount;
                    }
                    break;
                case FIELD_VOLUME:
                    _editAlarm.volume = constrain(_editAlarm.volume + delta, 0, 30);
                    break;
                case FIELD_REPEAT: {
                    int modeIdx = (int)_editAlarm.mode;
                    modeIdx = (modeIdx + delta + 5) % 5;
                    _editAlarm.mode = (AlarmManager::RepeatMode)modeIdx;
                    switch (_editAlarm.mode) {
                        case AlarmManager::REPEAT_DAILY:    _editAlarm.dayMask = 0x7F; break;
                        case AlarmManager::REPEAT_WEEKDAYS: _editAlarm.dayMask = 0x3E; break;
                        case AlarmManager::REPEAT_WEEKEND:  _editAlarm.dayMask = 0x41; break;
                        case AlarmManager::REPEAT_WEEKLY:   _editAlarm.dayMask = 0x02; break;
                        case AlarmManager::REPEAT_CUSTOM:   _editAlarm.dayMask = 0x7F; break;
                    }
                    break;
                }
                case FIELD_SNOOZE:
                    _editAlarm.snoozeMinutes = constrain(_editAlarm.snoozeMinutes + delta, 0, 120);
                    break;
                case FIELD_ENABLED:
                    _editAlarm.enabled = !_editAlarm.enabled;
                    break;
                default:
                    break;
            }
        } else {
            _selectedField += delta;
            if (_selectedField < 0) _selectedField = FIELD_COUNT - 1;
            if (_selectedField >= FIELD_COUNT) _selectedField = 0;
        }
    }
    else if (_screen == SCREEN_CONFIRM) {
        _listSelection = (_listSelection + delta + 2) % 2;
    }
    _needsRedraw = true;
}

void AlarmSetup::onButtonPress() {
    if (!_active) return;
    
    if (_screen == SCREEN_LIST) {
        if (_listSelection == 0) {
            stop();
            return;
        }
        int alarmCount = _alarmMgr->getCount();
        if (_listSelection == alarmCount + 1) {
            // Nieuw alarm
            _editAlarm = AlarmManager::AlarmEntry();
            _editAlarm.id = 0;
            _editAlarm.hour = 7;
            _editAlarm.minute = 0;
            _editAlarm.streamIndex = 0;
            _editAlarm.volume = 12;
            _editAlarm.mode = AlarmManager::REPEAT_DAILY;
            _editAlarm.dayMask = 0x7F;
            _editAlarm.snoozeMinutes = 10;
            _editAlarm.enabled = true;
            _selectedAlarmIndex = -1;
            _screen = SCREEN_EDIT;
            _selectedField = 0;
            _editMode = false;
        } else if (_listSelection > 0 && _listSelection <= alarmCount) {
            _editAlarm = _alarmMgr->getAlarms()[_listSelection - 1];
            _selectedAlarmIndex = _listSelection - 1;
            _screen = SCREEN_EDIT;
            _selectedField = 0;
            _editMode = false;
        }
    }
    else if (_screen == SCREEN_EDIT) {
        if (_selectedField == FIELD_BACK) {
            _screen = SCREEN_LIST;
            _listSelection = 0;
            _editMode = false;
        } else if (_selectedField == FIELD_SAVE) {
            saveAlarm();
            _screen = SCREEN_LIST;
            _listSelection = 0;
        } else if (_selectedField == FIELD_CANCEL) {
            _screen = SCREEN_LIST;
            _listSelection = (_selectedAlarmIndex >= 0) ? (_selectedAlarmIndex + 1) : 0;
        } else {
            _editMode = !_editMode;
        }
    }
    else if (_screen == SCREEN_CONFIRM) {
        if (_listSelection == 0) deleteAlarm();
        _screen = SCREEN_LIST;
        _listSelection = 0;
    }
    _needsRedraw = true;
}

void AlarmSetup::loop() {
    if (!_active || !_needsRedraw) return;
    draw();
    _needsRedraw = false;
}

void AlarmSetup::draw() {
    bool screenChanged = (_screen != _prevScreen);
    
    if (_firstDraw || screenChanged) {
        _tft->fillScreen(BG_COLOR);
        _firstDraw = false;
        _prevScreen = _screen;
        switch (_screen) {
            case SCREEN_LIST:   drawAlarmList(true); break;
            case SCREEN_EDIT:   drawEditForm(true); break;
            case SCREEN_CONFIRM: drawConfirmDialog(); break;
        }
    } else {
        switch (_screen) {
            case SCREEN_LIST:   drawAlarmList(false); break;
            case SCREEN_EDIT:   drawEditForm(false); break;
            case SCREEN_CONFIRM: drawConfirmDialog(); break;
        }
    }
    
    _prevListSelection = _listSelection;
    _prevField = _selectedField;
    _prevEditMode = _editMode;
}

void AlarmSetup::drawHeader(const char* title) {
    _tft->fillRect(0, 0, 480, 40, HEADER_BG);
    _tft->setTextFont(1);        // <-- TOEVOEGEN
    _tft->setTextColor(HEADER_FG, HEADER_BG);
    _tft->setTextDatum(MC_DATUM);
    _tft->setTextSize(1);
    _tft->drawString(title, 240, 20);
}

void AlarmSetup::drawAlarmList(bool fullRedraw) {
    int totalItems = _alarmMgr->getCount() + 2;
    bool pageChanged = updatePageOffset(_listSelection, totalItems, VISIBLE_ROWS, _scrollOffset);

    if (fullRedraw) drawHeader("Alarmen");

    if (fullRedraw || pageChanged) {
        for (int row = 0; row < VISIBLE_ROWS; row++) {
            int idx = _scrollOffset + row;
            if (idx < totalItems) {
                drawAlarmListItem(idx, (idx == _listSelection));
            } else {
                int y = LIST_START_Y + row * ITEM_HEIGHT;
                _tft->fillRect(10, y + 2, 460, ITEM_HEIGHT - 4, BG_COLOR);
            }
        }
    } else if (_prevListSelection != _listSelection && _prevListSelection >= 0) {
        drawAlarmListItem(_prevListSelection, false);
        drawAlarmListItem(_listSelection, true);
    }

    if (fullRedraw) {
        _tft->setTextColor(TFT_LIGHTGREY, BG_COLOR);
        _tft->setTextDatum(BC_DATUM);
        _tft->setTextSize(1);
        _tft->drawString("Draai = selecteer  |  Druk = bewerk  |  Lang = terug", 240, 315);
    }
}

void AlarmSetup::drawAlarmListItem(int index, bool selected) {
    int row = index - _scrollOffset;
    if (row < 0 || row >= VISIBLE_ROWS) return;

    int alarmCount = _alarmMgr->getCount();
    int y = LIST_START_Y + row * ITEM_HEIGHT;
    int itemHeight = ITEM_HEIGHT;
    
    _tft->fillRect(10, y + 2, 460, itemHeight - 4, selected ? HIGHLIGHT_COLOR : BG_COLOR);
    _tft->setTextFont(1);        // <-- TOEVOEGEN
    _tft->setTextDatum(ML_DATUM);
    _tft->setTextSize(2);
    _tft->setTextColor(FG_COLOR, selected ? HIGHLIGHT_COLOR : BG_COLOR);
    
    if (index == 0) {
        _tft->drawString("... Terug", 20, y + itemHeight / 2);
    } else if (index == alarmCount + 1) {
        _tft->drawString("+ Nieuw alarm instellen", 20, y + itemHeight / 2);
    } else if (index > 0 && index <= alarmCount) {
        const auto& alarm = _alarmMgr->getAlarms()[index - 1];
        char buf[64];
        const char* status = alarm.enabled ? "AAN" : "UIT";
        snprintf(buf, sizeof(buf), "%02d:%02d  %s  [%s]", 
                 alarm.hour, alarm.minute, 
                 getRepeatLabel(alarm.mode), status);
        _tft->drawString(buf, 20, y + itemHeight / 2);
    }
}

void AlarmSetup::drawEditForm(bool fullRedraw) {
    if (fullRedraw) {
        drawHeader(_selectedAlarmIndex >= 0 ? "Alarm Bewerken" : "Nieuw Alarm");
        for (int i = 0; i < FIELD_COUNT; i++) {
            drawEditField(i, (i == _selectedField), _editMode && (i == _selectedField));
        }
        _tft->setTextColor(TFT_LIGHTGREY, BG_COLOR);
        _tft->setTextDatum(BC_DATUM);
        _tft->setTextSize(1);
        _tft->drawString("Draai = aanpassen  |  Druk = bevestig/veld  |  Lang = terug", 240, 315);
    } else {
        if (_prevField != _selectedField || _prevEditMode != _editMode) {
            if (_prevField >= 0) drawEditField(_prevField, false, false);
            drawEditField(_selectedField, true, _editMode);
        } else if (_editMode && _selectedField == _prevField) {
            drawEditField(_selectedField, true, true);
        }
    }
}

void AlarmSetup::drawEditField(int field, bool selected, bool editing) {
    int startY = 50;
    int itemHeight = 30;
    int y = startY + field * itemHeight;

    uint16_t bg = selected ? (editing ? EDIT_COLOR : HIGHLIGHT_COLOR) : BG_COLOR;
    _tft->fillRect(10, y + 2, 460, itemHeight - 4, bg);
    _tft->setTextColor(FG_COLOR, bg);
    _tft->setTextFont(1);
    _tft->setTextDatum(ML_DATUM);
    _tft->setTextSize(2);
    
    char buf[64];
    const char* label = "";
    const char* value = "";
    
    switch (field) {
        case FIELD_BACK:
            label = "... Terug";
            break;
        case FIELD_HOUR:
            label = "Tijd (uur)";
            snprintf(buf, sizeof(buf), "%02d", _editAlarm.hour);
            value = buf;
            break;
        case FIELD_MINUTE:
            label = "Tijd (minuut)";
            snprintf(buf, sizeof(buf), "%02d", _editAlarm.minute);
            value = buf;
            break;
        case FIELD_STREAM:
            label = "Stream";
            if (_editAlarm.streamIndex < _urlMgr->streamCount) {
                value = _urlMgr->Streams[_editAlarm.streamIndex].name.c_str();
            } else {
                value = "Geen";
            }
            break;
        case FIELD_VOLUME:
            label = "Volume";
            snprintf(buf, sizeof(buf), "%d", _editAlarm.volume);
            value = buf;
            break;
        case FIELD_REPEAT:
            label = "Herhalen";
            value = getRepeatLabel(_editAlarm.mode);
            break;
        case FIELD_SNOOZE:
            label = "Snooze";
            snprintf(buf, sizeof(buf), "%d min", _editAlarm.snoozeMinutes);
            value = buf;
            break;
        case FIELD_ENABLED:
            label = "Status";
            value = _editAlarm.enabled ? "AAN" : "UIT";
            break;
        case FIELD_SAVE:
            label = ">> OPSLAAN <<";
            break;
        case FIELD_CANCEL:
            label = ">> ANNULEREN <<";
            break;
    }
    
    char out[128];
    if (strlen(value) > 0) {
        snprintf(out, sizeof(out), "%s: %s", label, value);
    } else {
        snprintf(out, sizeof(out), "%s", label);
    }
    _tft->drawString(out, 20, y + 15);
}

void AlarmSetup::drawConfirmDialog() {
    drawHeader("Alarm Verwijderen?");
    _tft->setTextFont(1);        // <-- TOEVOEGEN
    _tft->setTextColor(FG_COLOR, BG_COLOR);
    _tft->setTextDatum(MC_DATUM);
    _tft->setTextSize(2);
    _tft->drawString("Weet je zeker dat je", 240, 100);
    _tft->drawString("dit alarm wilt verwijderen?", 240, 130);
    
    int startY = 180;
    int itemHeight = 50;
    
    _tft->fillRect(100, startY, 280, itemHeight, _listSelection == 0 ? HIGHLIGHT_COLOR : BG_COLOR);
    _tft->setTextFont(1);        // <-- TOEVOEGEN (voor de knoppen)
    _tft->setTextColor(FG_COLOR, _listSelection == 0 ? HIGHLIGHT_COLOR : BG_COLOR);
    _tft->drawString("JA, verwijder", 240, startY + itemHeight / 2);
    
    _tft->fillRect(100, startY + itemHeight + 10, 280, itemHeight, _listSelection == 1 ? HIGHLIGHT_COLOR : BG_COLOR);
    _tft->setTextFont(1);        // <-- TOEVOEGEN (voor de knoppen)
    _tft->setTextColor(FG_COLOR, _listSelection == 1 ? HIGHLIGHT_COLOR : BG_COLOR);
    _tft->drawString("NEE, behoud", 240, startY + itemHeight + 10 + itemHeight / 2);
}

void AlarmSetup::saveAlarm() {
    AlarmManager::AlarmEntry newAlarms[AlarmManager::MAX_ALARMS];
    int count = _alarmMgr->getCount();
    
    if (_selectedAlarmIndex >= 0) {
        for (int i = 0; i < count; i++) newAlarms[i] = _alarmMgr->getAlarms()[i];
        newAlarms[_selectedAlarmIndex] = _editAlarm;
    } else {
        if (count >= AlarmManager::MAX_ALARMS) {
            Serial.println("AlarmSetup: max alarmen bereikt!");
            return;
        }
        for (int i = 0; i < count; i++) newAlarms[i] = _alarmMgr->getAlarms()[i];
        uint8_t maxId = 0;
        for (int i = 0; i < count; i++) {
            if (_alarmMgr->getAlarms()[i].id > maxId) maxId = _alarmMgr->getAlarms()[i].id;
        }
        _editAlarm.id = maxId + 1;
        newAlarms[count] = _editAlarm;
        count++;
    }
    
    JsonDocument doc;
    JsonArray arr = doc["alarms"].to<JsonArray>();
    for (int i = 0; i < count; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = newAlarms[i].id;
        obj["enabled"] = newAlarms[i].enabled;
        obj["hour"] = newAlarms[i].hour;
        obj["minute"] = newAlarms[i].minute;
        obj["streamIndex"] = newAlarms[i].streamIndex;
        obj["volume"] = newAlarms[i].volume;
        obj["mode"] = alarmModeToString(newAlarms[i].mode);
        obj["dayMask"] = newAlarms[i].dayMask;
        obj["snoozeMinutes"] = newAlarms[i].snoozeMinutes;
    }
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    String errorMsg;
    bool ok = _alarmMgr->updateFromJson((uint8_t*)jsonStr.c_str(), jsonStr.length(), _urlMgr->streamCount, errorMsg);
    Serial.println(ok ? "AlarmSetup: opgeslagen" : "AlarmSetup: fout " + errorMsg);
}

void AlarmSetup::deleteAlarm() {
    if (_selectedAlarmIndex < 0 || _selectedAlarmIndex >= _alarmMgr->getCount()) return;
    int count = _alarmMgr->getCount();
    AlarmManager::AlarmEntry newAlarms[AlarmManager::MAX_ALARMS];
    int j = 0;
    for (int i = 0; i < count; i++) {
        if (i != _selectedAlarmIndex) newAlarms[j++] = _alarmMgr->getAlarms()[i];
    }
    count = j;
    
    JsonDocument doc;
    JsonArray arr = doc["alarms"].to<JsonArray>();
    for (int i = 0; i < count; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = newAlarms[i].id;
        obj["enabled"] = newAlarms[i].enabled;
        obj["hour"] = newAlarms[i].hour;
        obj["minute"] = newAlarms[i].minute;
        obj["streamIndex"] = newAlarms[i].streamIndex;
        obj["volume"] = newAlarms[i].volume;
        obj["mode"] = alarmModeToString(newAlarms[i].mode);
        obj["dayMask"] = newAlarms[i].dayMask;
        obj["snoozeMinutes"] = newAlarms[i].snoozeMinutes;
    }
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    String errorMsg;
    _alarmMgr->updateFromJson((uint8_t*)jsonStr.c_str(), jsonStr.length(), _urlMgr->streamCount, errorMsg);
}

const char* AlarmSetup::getRepeatLabel(AlarmManager::RepeatMode mode) {
    switch (mode) {
        case AlarmManager::REPEAT_DAILY:    return "Dagelijks";
        case AlarmManager::REPEAT_WEEKDAYS: return "Werkdagen";
        case AlarmManager::REPEAT_WEEKEND:  return "Weekend";
        case AlarmManager::REPEAT_WEEKLY:   return "Wekelijks";
        case AlarmManager::REPEAT_CUSTOM:   return "Aangepast";
        default: return "Onbekend";
    }
}