#include "PrioScrollText.h"


ScrollText::ScrollText(TFT_eSPI& _tft, const String& _text, int16_t _x, int16_t _y, uint16_t _width, uint16_t _height, uint8_t _fontSize)
    : tft(_tft), text(_text), x(_x), y(_y), width(_width), height(_height), fontSize(_fontSize),
      lastScrollTime(0), textColor(TFT_WHITE), bgColor(TFT_BLACK), font(nullptr), textChanged(true) {
    createSprite();
    updateTextWidth();
    Serial.begin(115200);
    Serial.println("ScrollText initialized");
    Serial.print("Text width: ");
    Serial.println(textWidth);
}

ScrollText::~ScrollText() {
    destroySprite();
}

void ScrollText::createSprite() {
    sprite = new TFT_eSprite(&tft);
    sprite->createSprite(width, height);
    sprite->setColorDepth(8);
}

void ScrollText::destroySprite() {
    if (sprite) {
        sprite->deleteSprite();
        delete sprite;
    }
}

void ScrollText::drawTextToSprite() {
    sprite->fillSprite(bgColor);
    sprite->setTextColor(textColor);
    sprite->setTextSize(fontSize);
    if (font) {
        sprite->setFreeFont(font);
    }
    sprite->setCursor(width, (height - sprite->fontHeight()) / 2);
    sprite->print(text);
}

void ScrollText::updateTextWidth() {
    tft.setTextSize(fontSize);
    if (font) {
        tft.setFreeFont(font);
    }
    textWidth = tft.textWidth(text);
}

void ScrollText::setColors(uint16_t _textColor, uint16_t _bgColor) {
    textColor = _textColor;
    bgColor = _bgColor;
    textChanged = true;
}

void ScrollText::setText(const String& _text) {
    text = _text;
    updateTextWidth();
    textChanged = true;
    Serial.println("Text changed");
    Serial.print("New text width: ");
    Serial.println(textWidth);
}

void ScrollText::setFont(const GFXfont* _font) {
    font = _font;
    updateTextWidth();
    textChanged = true;
    Serial.println("Font changed");
    Serial.print("New text width: ");
    Serial.println(textWidth);
}

void ScrollText::setFontSize(uint8_t _fontSize) {
    fontSize = _fontSize;
    updateTextWidth();
    textChanged = true;
    Serial.println("Font size changed");
    Serial.print("New text width: ");
    Serial.println(textWidth);
}

void ScrollText::update() {
    unsigned long currentTime = millis();

    if (currentTime - lastScrollTime >= 30) {
        lastScrollTime = currentTime;

        if (textChanged) {
            drawTextToSprite();
            textChanged = false;
        }

        sprite->scroll(-1);  // Scroll 1 pixel naar links
  //      sprite->fillRect(width - 1, 0, 1, height, bgColor);  // Wis de rechterkant
        
        // Als we het einde van de tekst bereiken, teken dan opnieuw
        if (sprite->getCursorX() + textWidth <= width) {
            sprite->setCursor(width, (height - sprite->fontHeight()) / 2);
            sprite->print(text);
        }

        // Push sprite to screen
        sprite->pushSprite(x, y);
    }
}