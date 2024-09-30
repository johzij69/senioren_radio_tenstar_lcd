#include "PrioScrollText.h"


ScrollText::ScrollText(TFT_eSPI& _tft, const String& _text, int16_t _x, int16_t _y, uint16_t _width, uint16_t _height, uint8_t _fontSize)
    : tft(_tft), text(_text), x(_x), y(_y), width(_width), height(_height), fontSize(_fontSize),
      scrollPosition(0), lastScrollTime(0), textColor(TFT_WHITE), bgColor(TFT_BLACK), font(nullptr), textChanged(true) {
    createSprites();
    updateTextWidth();
    Serial.begin(115200);
    Serial.println("ScrollText initialized");
    Serial.print("Text width: ");
    Serial.println(textWidth);
}

ScrollText::~ScrollText() {
    destroySprites();
}

void ScrollText::createSprites() {
    sprite1 = new TFT_eSprite(&tft);
    sprite2 = new TFT_eSprite(&tft);
    sprite1->createSprite(width, height);
    sprite2->createSprite(width, height);
}

void ScrollText::destroySprites() {
    if (sprite1) {
        sprite1->deleteSprite();
        delete sprite1;
    }
    if (sprite2) {
        sprite2->deleteSprite();
        delete sprite2;
    }
}

void ScrollText::drawTextToSprite(TFT_eSprite* sprite, int16_t xPos) {
    sprite->fillSprite(bgColor);
    sprite->setTextColor(textColor);
    sprite->setTextSize(fontSize);
    if (font) {
        sprite->setFreeFont(font);
    }
    sprite->setCursor(xPos, (height - sprite->fontHeight()) / 2);
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
    scrollPosition = 0;
    textChanged = true;
    Serial.println("Text changed");
    Serial.print("New text width: ");
    Serial.println(textWidth);
}

void ScrollText::setFont(const GFXfont* _font) {
    font = _font;
    updateTextWidth();
    scrollPosition = 0;
    textChanged = true;
    Serial.println("Font changed");
    Serial.print("New text width: ");
    Serial.println(textWidth);
}

void ScrollText::setFontSize(uint8_t _fontSize) {
    fontSize = _fontSize;
    updateTextWidth();
    scrollPosition = 0;
    textChanged = true;
    Serial.println("Font size changed");
    Serial.print("New text width: ");
    Serial.println(textWidth);
}

void ScrollText::update() {
    unsigned long currentTime = millis();

    if (currentTime - lastScrollTime >= 30) {
        lastScrollTime = currentTime;

        if (textChanged || scrollPosition >= textWidth) {
            scrollPosition = 0;
            textChanged = false;
        }

        // Draw text to both sprites
        drawTextToSprite(sprite1, width - scrollPosition);
        drawTextToSprite(sprite2, width * 2 - scrollPosition);

        // Push sprites to screen
        sprite1->pushSprite(x, y);
        sprite2->pushSprite(x + width, y);

        scrollPosition++;

        Serial.print("Scroll position: ");
        Serial.println(scrollPosition);
    }
}