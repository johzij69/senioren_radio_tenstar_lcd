#include "PrioScrollText.h"


ScrollText::ScrollText(TFT_eSPI& _tft, const String& _text, int16_t _x, int16_t _y, uint16_t _width, uint16_t _height, uint8_t _fontSize)
    : tft(_tft), text(_text), x(_x), y(_y), width(_width), height(_height), fontSize(_fontSize),
      scrollPosition(0), lastScrollTime(0), startDelay(3000), pauseDelay(3000),
      textColor(TFT_WHITE), bgColor(TFT_BLACK), textChanged(true) {
    tft.setTextWrap(false);
    tft.setTextSize(fontSize);
    textWidth = tft.textWidth(text);
}

void ScrollText::setColors(uint16_t _textColor, uint16_t _bgColor) {
    textColor = _textColor;
    bgColor = _bgColor;
}

void ScrollText::setText(const String& _text) {
    text = _text;
    tft.setTextSize(fontSize);
    textWidth = tft.textWidth(text);
    scrollPosition = 0;
    textChanged = true;
    lastScrollTime = millis();  // Reset the scroll time
}

void ScrollText::setFontSize(uint8_t _fontSize) {
    fontSize = _fontSize;
    tft.setTextSize(fontSize);
    textWidth = tft.textWidth(text);
    scrollPosition = 0;
    textChanged = true;
    lastScrollTime = millis();  // Reset the scroll time
}

// void ScrollText::update() {
//     unsigned long currentTime = millis();

//     if (currentTime - lastScrollTime >= 30) {  // Update every 30ms for smooth scrolling
//         lastScrollTime = currentTime;

//         if (scrollPosition == 0 && !textChanged && currentTime - lastScrollTime < startDelay) {
//             // Wait for initial delay, but only if the text hasn't just changed
//             return;
//         }

//         // Clear the scroll area
//         tft.fillRect(x, y, width, height, bgColor);

//         // Calculate the starting position for drawing
//         int16_t drawX = x - scrollPosition;

//         // Draw the text
//         tft.setTextColor(textColor);
//         tft.setTextSize(fontSize);
//         tft.setCursor(drawX, y + (height - tft.fontHeight()) / 2);
//         tft.print(text);

//         // If the text is wider than the display area, scroll it
//         if (textWidth > width) {
//             scrollPosition++;

//             // Reset scroll position when the text has scrolled completely
//             if (scrollPosition >= textWidth + width) {
//                 scrollPosition = 0;
//                 lastScrollTime = currentTime;  // Reset time for pause
//                 textChanged = false;  // Reset the text changed flag
//             }
//         } else {
//             // If text fits within the width, reset scroll position and text changed flag
//             scrollPosition = 0;
//             textChanged = false;
//         }
//     }
// }

void ScrollText::update() {
    unsigned long currentTime = millis();

    if (currentTime - lastScrollTime >= 30) {
        lastScrollTime = currentTime;

        // Clear the scroll area
        tft.fillRect(x, y, width, height, bgColor);

        // Calculate the starting position for drawing
        int16_t drawX = x - scrollPosition;

        // Draw the text
        tft.setTextColor(textColor);
        tft.setTextSize(fontSize);
        tft.setCursor(drawX, y + (height - tft.fontHeight()) / 2);
        tft.print(text);

        Serial.print("Drawing text at X: ");
        Serial.print(drawX);
        Serial.print(", Y: ");
        Serial.println(y + (height - tft.fontHeight()) / 2);

        // If the text is wider than the display area, scroll it
        if (textWidth > width) {
            if (textChanged || scrollPosition >= textWidth + width) {
                scrollPosition = 0;
                textChanged = false;
            } else {
                scrollPosition++;
            }
        } else {
            scrollPosition = 0;
            textChanged = false;
        }
    }
}