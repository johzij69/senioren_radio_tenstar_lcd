#ifndef PRIO_ROTARY_MENU_H
#define PRIO_ROTARY_MENU_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>

// Menu UI Colors
#define MENU_BG_COLOR    TFT_BLACK
#define MENU_FG_COLOR    TFT_WHITE
#define MENU_HIGHLIGHT   TFT_BLUE
#define MENU_HEADER_BG   TFT_NAVY
#define MENU_HEADER_FG   TFT_WHITE

typedef void (*ActionCallback)(const char* action);
typedef void (*MenuStateCallback)();

class PrioRotaryMenu {
public:
    PrioRotaryMenu(TFT_eSPI &tft);

    void setCallbacks(ActionCallback action,
                      MenuStateCallback open,
                      MenuStateCallback close);

    bool loadMenu(const char* jsonString);

    void onButtonPress();
    void onRotaryDelta(int delta);

    bool isOpen();

    void loop();

private:
    TFT_eSPI* _tft;
    JsonDocument _menuDoc;

    ActionCallback _actionCallback = nullptr;
    MenuStateCallback _openCallback = nullptr;
    MenuStateCallback _closeCallback = nullptr;

    bool _isOpen = false;
    volatile bool _needsRedraw = true;
    bool _stateChanged = true;
    int _menuDelta = 0;

    enum MenuState {
        MAIN_MENU,
        SUB_MENU
    };

    MenuState _state = MAIN_MENU;

    int _selectedMainIndex = 0;
    int _selectedSubIndex = 0;
    int _currentCategoryIndex = 0;

    // Logic
    void handleSelection();
    void updateSelection();

    // Rendering
    void drawMenu();
    void drawHeader(const char* title);
    void drawMainMenuItems();
    void drawSubMenuItems();
};

#endif // PRIO_ROTARY_MENU_H