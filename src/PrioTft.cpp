#include "PrioTft.h"

PrioTft::PrioTft() : tft(), pBar(&tft), max_volume(30), last_volume(10), cur_volume(0), isInitialized(false), sTitle(tft, 20, 60, 100, 450, FM12)
{
    // Constructor body (indien nodig)
}

void PrioTft::begin()
{
    tft.init();         // Initialiseer het scherm
    tft.setRotation(3); // Pas rotatie aan indien nodig
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0, 4);
    tft.setTextColor(TFT_WHITE);
    tft.println("PRIO-WEBRADIO");
    pBar.begin(450, 0, 30, 320, TFT_LIGHTGREY, TFT_BLACK, max_volume);
    pBar.draw(last_volume);
    isInitialized = true; // Scherm is geïnitialiseerd
    cur_volume = last_volume;
    sTitle.begin();
    sTitle.setScrollerText("Dit is een lange tekst die zal scrollen, zodat je de hele tekst kunt lezen.");
}

void PrioTft::loop()
{
    pBar.draw(cur_volume);
    sTitle.loop();
}

void PrioTft::showLocalIp(const String &ip)
{
    tft.println("IP address: " + ip);
}

void PrioTft::setVolume(int _cur_volume)
{
    cur_volume = _cur_volume;
}
