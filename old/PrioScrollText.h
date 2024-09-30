#ifndef PRIOSCROLL_TEXT_H
#define PRIOSCROLL_TEXT_H

#include <TFT_eSPI.h>

class ScrollText {
private:
    TFT_eSPI& tft;
    TFT_eSprite* sprite;
    String text;
    int16_t x, y;
    uint16_t width, height;
    int16_t textWidth;
    unsigned long lastScrollTime;
    uint16_t textColor;
    uint16_t bgColor;
    uint8_t fontSize;
    const GFXfont* font;
    bool textChanged;

    void createSprite();
    void destroySprite();
    void drawTextToSprite();
    void updateTextWidth();

public:
    ScrollText(TFT_eSPI& _tft, const String& _text, int16_t _x, int16_t _y, uint16_t _width, uint16_t _height, uint8_t _fontSize);
    ~ScrollText();
    void setColors(uint16_t _textColor, uint16_t _bgColor);
    void setText(const String& _text);
    void setFont(const GFXfont* _font);
    void setFontSize(uint8_t _fontSize);
    void update();
};

#endif // PRIOSCROLL_TEXT_H