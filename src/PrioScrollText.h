#ifndef PRIOSCROLL_TEXT_H
#define PRIOSCROLL_TEXT_H

#include <TFT_eSPI.h>

enum class ScrollDirection
{
    LEFT_TO_RIGHT_TO_LEFT,
    RIGHT_TO_LEFT_ONCE,
    RIGHT_TO_LEFT_CONT,
    TOP_TO_BOTTOM,
    BOTTOM_TO_TOP
};

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
    int scrollSpeed;

    uint16_t textColor;
    uint16_t bgColor;
    uint8_t fontSize;
    const GFXfont *font;
    bool textChanged;
    bool fullScreenScroll;
    bool descending;

    // Palette colour table
    uint16_t palette[16];

    ScrollDirection scrollDirection;
    unsigned long nowMillis;
    unsigned long startMillis;
    unsigned long scrollDelay;

    int ani = 0;
    int tcount = 0;

    int scrollValue;

    void createSprite();
    void destroySprite();
    void updateTextWidth();
    void scrollLeftCont();
    void scrollLeftOnce();
    void scrollLeftRightLeft();

public:
    ScrollText(TFT_eSPI &_tft, const String &_text, int16_t _xPos, int16_t _yPos, uint16_t _width, uint16_t _height, const GFXfont *_font);
    ~ScrollText();
    void begin();
    void setColors(uint16_t _textColor, uint16_t _bgColor);
    void setText(const String &_text);
    void setFont(const GFXfont *_font);
    void setFontSize(uint8_t _fontSize);
    void setTextBetweenSpace(int _textBetweenSpace);
    void setScrollDirection(ScrollDirection direction);
    void setScrollDelay(unsigned long _scrollDelay);
    void setScrollSpeed(int _scrollSpeed);
    void update();

    bool debug;
};

#endif // PRIOSCROLL_TEXT_H