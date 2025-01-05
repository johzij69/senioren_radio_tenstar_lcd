#include "TFTScroller.h"

TFTScroller::TFTScroller(TFT_eSPI &tft, uint16_t _xPos, uint16_t _yPos, uint8_t scrollDelay, uint16_t viewportWidth, const GFXfont *_font)
    : tft(tft), sprite(&tft), scrollerText(""), arrayPos(0),yPos(_yPos), xPos(_xPos),
      startMillis(0), scrollDelay(scrollDelay), fontWidth(fontWidth), viewportWidth(viewportWidth),font(_font) {}

void TFTScroller::begin()
{
    tft.setSwapBytes(true);
    sprite.setTextWrap(false);
    setFont(font);
    sprite.setTextSize(1);
    sprite.setTextColor(0xFFFF, 0x0000);

    fontWidth = sprite.textWidth("W");
    charWidthCounter = fontWidth;
    viewportWidth = viewportWidth - fontWidth - xPos;
    sprite.createSprite(viewportWidth + fontWidth, 32);
}

void TFTScroller::setScrollerText(const String &text)
{
    scrollerText = text;
    arrayPos = 0;
}

void TFTScroller::setFont(const GFXfont *_font)
{
    font = _font;
    sprite.setFreeFont(font);
}

void TFTScroller::loop()
{
    if (millis() - startMillis >= scrollDelay)
    {
        scrollLogic();
        startMillis = millis();
    }
}



// todo scrollLogic en drawCharacter samenvoegen

void TFTScroller::scrollLogic()
{
    uint16_t arraySize = scrollerText.length() + 1;
    char stringArray[arraySize];
    scrollerText.toCharArray(stringArray, arraySize);

    if (arrayPos >= arraySize)
    {
        arrayPos = 0;
    }

    sprite.pushSprite(xPos, yPos);
    sprite.scroll(-1);  // scroll 1 pixel left

    // als de counter op 0 staat, dan moet er een nieuwe letter getekend worden
    if (--charWidthCounter <= 0)
    {
        drawCharacter();
    }
}

void TFTScroller::drawCharacter()
{
    uint16_t arraySize = scrollerText.length() + 1;
    char stringArray[arraySize];
    scrollerText.toCharArray(stringArray, arraySize);
    char character = stringArray[arrayPos];

    charWidthCounter = fontWidth;
    sprite.drawString(String(character), viewportWidth, 0, 1);

    arrayPos++;
             
}