// PrioTft.h
#ifndef PRIOTFT_H
#define PRIOTFT_H

#define TFT_GREYBLUE 0x3DB9

#include <TFT_eSPI.h> // Zorg ervoor dat je TFT_eSPI of een andere bibliotheek gebruikt
#include "PrioBar.h"
#include "TFTScroller.h"
#include "StreamLogo.h"

class PrioTft {
private:
    TFT_eSPI tft; // TFT scherm object


public:
    PrioBar pBar; // Volume balk object
    TFTScroller sTitle; // Scrollende titel object
    StreamLogo sLogo;
    bool isInitialized = false; // Is het scherm geïnitialiseerd?
    int max_volume = 30; // Maximale volume waarde
    int last_volume = 10; // Laatst ingestelde volume waarde
    int cur_volume = 0; // Huidige volume waarde
    int kantline = 25 ;
    
    PrioTft();
    void begin(); // Initialiseer het scherm
    void loop();  // Update het scherm
    void showLocalIp(const String &ip); // Toon het IP adres op het scherm
    void setVolume(int _cur_volume);
    void setTitle(const String &title);
    void setLogo(const String &url);
    void setStreamTitle(const String &streamName);
    void clearLine(int lineNumber);
    void clearLogoLine(int lineNumber);
    void prepLine(int lineNumber);
    void prepLogoLine(int lineNumber);
    void clearGreenLine(int lineNumber);
    void showTime(const String &time);
    void drawParaLine(const String &text, int startY);
    String truncateStringToFit(const String &text, int maxWidth);
};

#endif // PRIOTFT_H
