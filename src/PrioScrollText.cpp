// see https://github.com/Bodmer/TFT_eSPI/blob/master/examples/Sprite/Sprite_scroll_4bit/Sprite_scroll_4bit.ino

#include "PrioScrollText.h"
#include "arrow.h"
#include "Free_Fonts.h"

ScrollText::ScrollText(TFT_eSPI &_tft, const String &_text, int16_t _xPos, int16_t _yPos, uint16_t _width, uint16_t _height, const GFXfont *_font)
    : tft(_tft), text(_text), xPos(_xPos), yPos(_yPos), width(_width), height(_height), font(_font),
      lastScrollTime(0), textColor(TFT_WHITE), bgColor(TFT_BLACK), textChanged(true)
{

    maxTextWidth = 960;
    textBetweenSpace = 0;
    textChanged = true;
    fullScreenScroll = true;
    sprite = new TFT_eSprite(&tft);


}

ScrollText::~ScrollText()
{
    destroySprite();
}

void ScrollText::begin()
{
    setFont(font);

    if (tft.width() >= textWidth)
    {
        fullScreenScroll = false;
        scrollSize = 0;
    }
    createSprite();
}    


void ScrollText::createPalette()
{
    // Populate the palette table, table must have 16 entries
    palette[0] = TFT_BLACK;
    palette[1] = TFT_ORANGE;
    palette[2] = TFT_DARKGREEN;
    palette[3] = TFT_DARKCYAN;
    palette[4] = TFT_MAROON;
    palette[5] = TFT_PURPLE;
    palette[6] = TFT_OLIVE;
    palette[7] = TFT_DARKGREY;
    palette[8] = TFT_ORANGE;
    palette[9] = TFT_BLUE;
    palette[10] = TFT_GREEN;
    palette[11] = TFT_CYAN;
    palette[12] = TFT_RED;
    palette[13] = TFT_NAVY;
    palette[14] = TFT_YELLOW;
    palette[15] = TFT_WHITE;
}

void ScrollText::createSprite()
{

    // // // Create a sprite for the graph
    graph1 = new TFT_eSprite(&tft);
    // graph1->setColorDepth(4);
    graph1->createSprite(450, 40);
    graph1->setSwapBytes(true);
    graph1->setTextColor(TFT_WHITE, TFT_BLACK);
    // graph1->createPalette(palette);
    // graph1->fillSprite(9); // Note: Sprite is filled with palette[0] colour when created
    // // scrolling text sprite

    //   tft.setFreeFont(FSS12); // In case you want to write non-scrolling text.
    //   textWidth = tft.textWidth(text);

    sprite->setColorDepth(1);
    sprite->createSprite(textWidth + tft.width(), tft.fontHeight()); // for now fixed size
    sprite->setTextColor(TFT_WHITE, TFT_BLACK);
    sprite->setTextSize(1); // Diit is de text vergrotingsfactor!
    Serial.print("text width: after create sprite");
    Serial.println(textWidth);
    Serial.print("screen width: ");
    Serial.println(tft.width());

    tft.drawString("text width:" + String(textWidth), 0, 100, GFXFF);        
    tft.drawString("scrollsize:" + String(scrollSize), 0, 120, GFXFF);        
    tft.drawString("font height:" + String(tft.fontHeight()), 0, 140, GFXFF); 
    tft.drawString("space between:" + String(textBetweenSpace), 0, 160, GFXFF); 
    

}

void ScrollText::destroySprite()
{
    if (sprite)
    {
        sprite->deleteSprite();
        delete sprite;
    }
}

void ScrollText::drawTextToSprite()
{
    sprite->fillSprite(bgColor);
    sprite->setTextColor(textColor);
    sprite->setTextSize(fontSize);
    if (font)
    {
        sprite->setFreeFont(font);
    }
    sprite->setCursor(width, (height - sprite->fontHeight()) / 2);
    sprite->print(text);
}

void ScrollText::updateTextWidth()
{
    textWidth = tft.textWidth(text);
    if (textWidth > maxTextWidth)
    {
        textWidth = maxTextWidth;
    }
    scrollSize = textWidth - tft.width();
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
    Serial.println("Text changed");
    Serial.print("New text width: ");
    Serial.println(textWidth);
}

void ScrollText::setFont(const GFXfont *_font)
{
    font = _font;
    textChanged = true;
    sprite->setFreeFont(font);
    tft.setFreeFont(font);
    updateTextWidth();
    Serial.println("Font changed");
    Serial.print("New text width: ");
    Serial.println(textWidth);
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
    Serial.println("Font size changed");
    Serial.print("New text width: ");
    Serial.println(textWidth);
}

void ScrollText::update()
{

    // ani--;
    // if (ani < -scrollSize)
    //     ani = 0;

    // graph1->fillSprite(TFT_BLACK);
    // sprite->drawString(text, 0, 0, 4);
    // sprite->pushToSprite(graph1, ani, 10, TFT_BLACK);
    // graph1->pushSprite(0, 0);

    sprite->pushSprite(0, yPos);
    if (fullScreenScroll)
    {
        sprite->scroll(-1); // scroll stext 1 pixel left, up/down default is 0
        if (textChanged)
        {
            textChanged = false;
            sprite->drawString(text, 0, 0, GFXFF);
            tcount = scrollSize + textBetweenSpace;
        }

        tcount--;
        if (tcount <= 0)
        {
            tcount = tft.width() + scrollSize + textBetweenSpace;
            sprite->drawString(text, tft.width(), 0, GFXFF);
        }
    }
    else
    {
        sprite->drawString(text, 0, 0, GFXFF);
    }
}

// void ScrollText::update()
// {
//     // Push the sprites onto the TFT at specified coordinates
//     //  graph1->pushSprite(0, 0);
//     // sprite->pushSprite(0, 0);

//  //   graph1->fillSprite(TFT_BLACK);
//  //   sprite->fillSprite(TFT_BLACK);
//  graph1->drawString("Hallo weerlef",0,0,2);
//     sprite->drawString("johan", 0, 0, 4); // draw at 6,0 in sprite, font 2
//     sprite->drawString("johan2", 230, 0, 4); // draw at 6,0 in sprite, font 2

//     sprite->pushToSprite(graph1, 10, 0, TFT_SKYBLUE);
//     Serial.print("New text width: ");
//     Serial.println(textWidth);
//     Serial.print("New text: ");
//     Serial.println(text);

//     Serial.print("boxwidth ");
//     Serial.println(width);

//     Serial.print("x:");
//     Serial.println(x);
//     Serial.print("y:");
//     Serial.println(y);

//     delay(50); // wait so things do not scroll too fast

//  //   graph1->pushSprite(0, 0);
//     sprite->pushSprite(tcount,20);

//     // Now scroll the sprites scroll(dt, dy) where:
//     // dx is pixels to scroll, left = negative value, right = positive value
//     // dy is pixels to scroll, up = negative value, down = positive value
//     //   graph1->scroll(-1, 0);  // scroll graph 1 pixel left, 0 up/down
//     //    stext1.scroll(0, -16); // scroll stext 0 pixels left/right, 16 up
//     //   sprite->scroll(-1, 0); // scroll stext 1 pixel right, up/down default is 0

//     // Draw the grid on far right edge of sprite as graph has now moved 1 pixel left

//     tcount--;
//     if (tcount <= -100)
//     { // If we have scrolled 40 pixels then redraw text
//         tcount = 450;
//         //     sprite->drawString(text, 200, 0, 2); // draw at 6,0 in sprite, font 2
//     }
// }

// void ScrollText::update()
// {
//     unsigned long currentTime = millis();

//     if (currentTime - lastScrollTime >= 30)
//     {
//         lastScrollTime = currentTime;

//         if (textChanged)
//         {
//             drawTextToSprite();
//             textChanged = false;
//         }

//         sprite->scroll(-1); // Scroll 1 pixel naar links
//                             //      sprite->fillRect(width - 1, 0, 1, height, bgColor);  // Wis de rechterkant

//         // Als we het einde van de tekst bereiken, teken dan opnieuw
//         if (sprite->getCursorX() + textWidth <= width)
//         {
//             sprite->setCursor(width, (height - sprite->fontHeight()) / 2);
//             sprite->print(text);
//         }

//         // Push sprite to screen
//         sprite->pushSprite(x, y);
//     }
// }