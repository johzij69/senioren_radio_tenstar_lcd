#ifndef PRIOSCROLL_TEXT_H
#define PRIOSCROLL_TEXT_H

#include <TFT_eSPI.h>

class ScrollText
{
private:
    TFT_eSPI &tft;
    TFT_eSprite *sprite;
    TFT_eSprite *graph1;

    String text;
    int16_t xPos, yPos;
    uint16_t width, height;
    int16_t textWidth;
    int16_t maxTextWidth;
    int16_t scrollSize;
    int textBetweenSpace;
    
    unsigned long lastScrollTime;
    uint16_t textColor;
    uint16_t bgColor;
    uint8_t fontSize;
    const GFXfont *font;
    bool textChanged;
    bool fullScreenScroll;

    // Palette colour table
    uint16_t palette[16];

int ani =0;

int tcount = 0;

    void createSprite();
    void destroySprite();
    void drawTextToSprite();
    void updateTextWidth();
    void createPalette();



public:
    ScrollText(TFT_eSPI &_tft, const String &_text, int16_t _xPos, int16_t _yPos, uint16_t _width, uint16_t _height, const GFXfont *_font);
    ~ScrollText();
    void begin();
    void setColors(uint16_t _textColor, uint16_t _bgColor);
    void setText(const String &_text);
    void setFont(const GFXfont *_font);
    void setFontSize(uint8_t _fontSize);
    void setTextBetweenSpace(int _textBetweenSpace);
    
    void update();
};

#endif // PRIOSCROLL_TEXT_H