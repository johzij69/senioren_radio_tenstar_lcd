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

     // Nieuw: openbaar sluiten, zodat je het menu ook buiten deze class kunt sluiten
    void closeMenu();


private:
    TFT_eSPI* _tft;
    JsonDocument _menuDoc;

    bool _isOpen = false;
    bool _stateChanged = true;
    volatile bool _needsRedraw = true;

    // note this wil make this class less general, but for me it is fine, as I only want to use it for the rotary encoder and button
    unsigned long _lastActionTime = 0; // Cooldown timer voor menu acties

    ActionCallback _actionCallback = nullptr;
    MenuStateCallback _openCallback = nullptr;
    MenuStateCallback _closeCallback = nullptr;


    int _menuDelta = 0;

    enum MenuState {
        MAIN_MENU,
        SUB_MENU
    };

    MenuState _state = MAIN_MENU;


    // Nieuw: bewaarde TFT tekstinstellingen
    struct SavedTextStyle {
        uint8_t  textSize  = 1;
        uint8_t  textDatum = TL_DATUM;
        uint8_t  textFont  = 1;
        uint16_t fgColor   = TFT_WHITE;
        uint16_t bgColor   = TFT_BLACK;
        int16_t  cursorX   = 0;
        int16_t  cursorY   = 0;
        bool     valid     = false;
    };

    int _selectedMainIndex = 0;
    int _selectedSubIndex = 0;
    int _currentCategoryIndex = 0;

    SavedTextStyle _savedTextStyle;

    void saveTextStyle();
    void restoreTextStyle();

    // Logic
    void handleSelection();
    void updateSelection();

    // Rendering
    void drawMenu();
    void drawHeader(const char* title);
    void drawFooter();
    void drawMainMenuItems();
    void drawSubMenuItems();
    void drawMenuItem(int y, int itemHeight, const char* label, bool selected);
    
};

#endif // PRIO_ROTARY_MENU_H