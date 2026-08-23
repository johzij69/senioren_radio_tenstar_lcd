#ifndef ALARM_SETUP_H
#define ALARM_SETUP_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "AlarmManager.h"
#include "UrlManager.h"

class AlarmSetup {
public:
    AlarmSetup(TFT_eSPI &tft, AlarmManager &alarmMgr, UrlManager &urlMgr);
    
    void start();
    bool isActive() const { return _active; }
    int getScreenAsInt() const { return (int)_screen; }
    void stop();
    void loop();
    
    void onRotaryDelta(int delta);
    void onButtonPress();
    
    // Wacht tot de fysieke knop los is voordat we input accepteren
    bool isWaitingForBtnRelease() const { return _waitForBtnRelease; }
    void clearWaitForBtnRelease() { _waitForBtnRelease = false; }

private:
    TFT_eSPI *_tft;
    AlarmManager *_alarmMgr;
    UrlManager *_urlMgr;
    
    bool _active = false;
    bool _needsRedraw = true;
    bool _editMode = false;
    bool _waitForBtnRelease = false;
    
    enum Screen { SCREEN_LIST, SCREEN_EDIT, SCREEN_CONFIRM };
    Screen _screen = SCREEN_LIST;
    Screen _prevScreen = SCREEN_LIST;
    
    enum EditField {
        FIELD_BACK,
        FIELD_HOUR,
        FIELD_MINUTE,
        FIELD_STREAM,
        FIELD_VOLUME,
        FIELD_REPEAT,
        FIELD_SNOOZE,
        FIELD_ENABLED,
        FIELD_SAVE,
        FIELD_CANCEL,
        FIELD_COUNT
    };
    
    AlarmManager::AlarmEntry _editAlarm;
    int _selectedAlarmIndex = -1;
    int _selectedField = 0;
    int _listSelection = 0;
    
    // Partial redraw state
    int _prevListSelection = -1;
    int _prevField = -1;
    bool _prevEditMode = false;
    bool _firstDraw = true;
    
    static const char* getRepeatLabel(AlarmManager::RepeatMode mode);
    
    void draw();
    void drawAlarmList(bool fullRedraw);
    void drawAlarmListItem(int index, bool selected);
    void drawEditForm(bool fullRedraw);
    void drawEditField(int field, bool selected, bool editing);
    void drawConfirmDialog();
    void drawHeader(const char* title);
    
    void saveAlarm();
    void deleteAlarm();
    
    static constexpr uint16_t BG_COLOR = TFT_BLACK;
    static constexpr uint16_t FG_COLOR = TFT_WHITE;
    static constexpr uint16_t HIGHLIGHT_COLOR = TFT_BLUE;
    static constexpr uint16_t EDIT_COLOR = TFT_ORANGE;
    static constexpr uint16_t HEADER_BG = TFT_NAVY;
    static constexpr uint16_t HEADER_FG = TFT_WHITE;
};

#endif