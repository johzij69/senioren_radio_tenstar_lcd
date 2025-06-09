// see https://github.com/Bodmer/TFT_eSPI/blob/master/examples/Sprite/Sprite_scroll_4bit/Sprite_scroll_4bit.ino

#include "PrioScrollText.h"
#include "Free_Fonts.h"

ScrollText::ScrollText(TFT_eSPI &_tft, const String &_text, int16_t _xPos, int16_t _yPos, uint16_t _width, uint16_t _height, const GFXfont *_font)
    : tft(_tft), text(_text), xPos(_xPos), yPos(_yPos), width(_width), height(_height), font(_font)
{

    // set defaults
    maxTextWidth = 960;
    textBetweenSpace = 0;
    textChanged = true;
    mustScroll = true;
    scrollDelay = 20;
    scrollType = ScrollType::RIGHT_TO_LEFT_CONT;
    scrollValue = -1;
    scrollSpeed = 1;
    descending = true;
    debug = false;
    tcount = 0;
    graph1 = new TFT_eSprite(&tft);
    sprite = new TFT_eSprite(&tft);
}

ScrollText::~ScrollText()
{
    destroySprite();
}

void ScrollText::begin()
{
    setFont(font);
    createSprite();
    updateTextWidth();

    if (graph1->width() >= textWidth)
    {
        mustScroll = false;
        scrollSize = 0;
    }

    if (debug)
    {
        tft.drawString("graph1 width:" + String(graph1->width()), 0, 220, GFXFF);
        tft.drawString("text width:" + String(textWidth), 0, 240, GFXFF);
        tft.drawString("font height:" + String(tft.fontHeight()), 0, 260, GFXFF);
        tft.drawString("space between:" + String(textBetweenSpace), 0, 280, GFXFF);
    }
}

void ScrollText::createSprite()
{
    graph1->setColorDepth(1);
    graph1->createSprite(width, tft.fontHeight());
    graph1->setTextColor(TFT_WHITE, TFT_BLACK);
    graph1->setTextSize(1); // Dit is de text vergrotingsfactor!

    sprite->setColorDepth(1);
    sprite->createSprite(textWidth + tft.width(), tft.fontHeight());
    sprite->setTextColor(TFT_WHITE, TFT_BLACK);
    sprite->setTextSize(1); // Dit is de text vergrotingsfactor!
}

void ScrollText::destroySprite()
{
    if (sprite)
    {
        sprite->deleteSprite();
        delete sprite;
    }
    if (graph1)
    {
        graph1->deleteSprite();
        delete graph1;
    }
}

void ScrollText::updateTextWidth()
{
    textWidth = tft.textWidth(text);
    if (textWidth > maxTextWidth)
    {
        textWidth = maxTextWidth;
    }
    scrollSize = textWidth - graph1->width();
}

void ScrollText::setColors(uint16_t _textColor, uint16_t _bgColor)
{
    textColor = _textColor;
    bgColor = _bgColor;
    textChanged = true;
}

void ScrollText::setText(const String &_text)
{
    text = _text;
    updateTextWidth();
    textChanged = true;
}

void ScrollText::setFont(const GFXfont *_font)
{
    font = _font;
    textChanged = true;
    sprite->setFreeFont(font);
    tft.setFreeFont(font);
    updateTextWidth();
}

void ScrollText::setTextBetweenSpace(int _textBetweenSpace)
{
    textBetweenSpace = _textBetweenSpace;
}

void ScrollText::setFontSize(uint8_t _fontSize)
{
    fontSize = _fontSize;
    updateTextWidth();
    textChanged = true;
}

void ScrollText::setScrollType(ScrollType _scrollType)
{
    scrollType = _scrollType;
    textChanged = true;
}

void ScrollText::setScrollDelay(unsigned long _scrollDelay)
{
    scrollDelay = _scrollDelay;
    textChanged = true;
}

void ScrollText::setScrollSpeed(int _scrollSpeed)
{
    scrollSpeed = _scrollSpeed;
    textChanged = true;
}

// Scrollers

void ScrollText::scrollLeftCont()
{
    sprite->scroll(-scrollSpeed); // scroll stext 1 pixel left, up/down default is 0
    if (textChanged)
    {
        textChanged = false;
        sprite->drawString(text, 0, 0, GFXFF);
        tcount = (scrollSize + textBetweenSpace ) / scrollSpeed;
    }

    if (tcount == 0)
    {
        sprite->drawString(text, width, 0, GFXFF);
        tcount = (textWidth + textBetweenSpace) / scrollSpeed;
    }
    tcount--;
}

void ScrollText::scrollLeftOnce()
{

    if (textChanged)
    {
        textChanged = false;
        sprite->drawString(text, 0, 0, GFXFF);
        tcount = (width + scrollSize) / scrollSpeed;
        startMillis = millis();
    }

    nowMillis = millis();

    if (tcount == 0)
        textChanged = true; // this will restart the scroll

    if (nowMillis >= startMillis + scrollDelay)
    {
        tcount--;
        sprite->scroll(-scrollSpeed);
    }
}

void ScrollText::scrollLeftRightLeft()
{

    if (textChanged)
    {
        textChanged = false;
        sprite->drawString(text, 0, 0, GFXFF);
        tcount = 0;
        descending = true;
        startMillis = millis();
    }
    sprite->drawString(text, tcount, 0, GFXFF);
    nowMillis = millis();

    if (nowMillis >= startMillis + scrollDelay)
    {
        if (descending)
        {
            tcount = tcount - scrollSpeed;
            if (tcount <= -scrollSize) // We can't do equal to scrollSize because of scrollSpeed step size which can be greater than 1
            {
                descending = false;
                tcount = -scrollSize;
                startMillis = millis();
            }
        }
        else
        {
            tcount = tcount + scrollSpeed;
            if (tcount >= 0) // We can't do equal to zero because of scrollSpeed step size which can be greater than 1
            {
                descending = true;
                tcount = 0;
                startMillis = millis();
            }
        }
    }
}

// Update loop, maybe call it loop?

void ScrollText::update()
{

    graph1->fillSprite(TFT_BLACK);
    sprite->pushToSprite(graph1, 0, 0, TFT_BLACK);
    graph1->pushSprite(xPos, yPos);

    if (mustScroll)
    {

        switch (scrollType)
        {
        case ScrollType::RIGHT_TO_LEFT_CONT:
            scrollLeftCont();
            break;
        case ScrollType::RIGHT_TO_LEFT_ONCE:
            scrollLeftOnce();
            break;
        case ScrollType::LEFT_TO_RIGHT_TO_LEFT:
            scrollLeftRightLeft();
            break;
            // case ScrollDirection::BOTTOM_TO_TOP:
            //     scrollUp();
            //     break;
        default:
            scrollLeftCont();
            break;
        }
    }
    else
    {
        sprite->drawString(text, 0, 0, GFXFF);
    }
}
