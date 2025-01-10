#ifndef TFT_SCROLLER_H
#define TFT_SCROLLER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Free_Fonts.h"

class TFTScroller
{
private:
    TFT_eSPI &tft;
    TFT_eSprite sprite;

    String scrollerText;

    unsigned long startMillis;

    uint16_t xPos, yPos;
    uint16_t arrayPos;
    uint16_t viewportWidth;

    uint8_t scrollDelay;
    uint8_t fontWidth;
    uint8_t charWidthCounter;

    const GFXfont *font;

    bool scrollingIsActive;

    void drawCharacter();
    void scrollLogic();

public:
    TFTScroller(TFT_eSPI &tft, uint16_t _xPos, uint16_t _yPos, uint8_t scrollDelay = 20, uint16_t viewportWidth = 320, const GFXfont *_font = FMBO12);
    void begin();
    void setScrollerText(const String &text);
    void setFont(const GFXfont *_font);
    void loop();
};

#endif