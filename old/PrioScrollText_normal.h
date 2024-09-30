#ifndef PRIOSCROLL_TEXT_H
#define PRIOSCROLL_TEXT_H

#include <TFT_eSPI.h>

class ScrollText {
private:
    TFT_eSPI& tft;
    String text;
    int16_t x, y;
    uint16_t width, height;
    int16_t textWidth;
    int16_t scrollPosition;
    unsigned long lastScrollTime;
    unsigned long startDelay;
    unsigned long pauseDelay;
    uint16_t textColor;
    uint16_t bgColor;
    uint8_t fontSize;
    bool textChanged;

public:
    ScrollText(TFT_eSPI& _tft, const String& _text, int16_t _x, int16_t _y, uint16_t _width, uint16_t _height, uint8_t _fontSize);
    void setColors(uint16_t _textColor, uint16_t _bgColor);
    void setText(const String& _text);
    void setFontSize(uint8_t _fontSize);
    void update();
};

#endif // PRIOSCROLL_TEXT_H