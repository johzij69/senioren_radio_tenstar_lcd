#ifndef PRIOSCROLL_TEXT_H
#define PRIOSCROLL_TEXT_H

#include <TFT_eSPI.h>


class ScrollText {
private:
    TFT_eSPI& tft;
    TFT_eSprite* sprite1;
    TFT_eSprite* sprite2;
    String text;
    int16_t x, y;
    uint16_t width, height;
    int16_t textWidth;
    int16_t scrollPosition;
    unsigned long lastScrollTime;
    uint16_t textColor;
    uint16_t bgColor;
    uint8_t fontSize;
    const GFXfont* font;
    bool textChanged;

    void createSprites();
    void destroySprites();
    void drawTextToSprite(TFT_eSprite* sprite, int16_t xPos);
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