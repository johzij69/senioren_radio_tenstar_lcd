#include "PrioTft.h"

PrioTft::PrioTft() : tft(), pBar(&tft), sLogo(&tft), max_volume(30), last_volume(10), cur_volume(0), isInitialized(false), sTitle(tft, 20, 60, 100, 450, FM12)
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
    sLogo.begin();
    sLogo.Show(35, 100, "https://img.prio-ict.nl/api/images/webradio-default.jpg");
    isInitialized = true; // Scherm is geïnitialiseerd
    cur_volume = last_volume;
    sTitle.begin();
    //  sTitle.setScrollerText("Dit is een lange tekst die zal scrollen, zodat je de hele tekst kunt lezen.");
    sTitle.setScrollerText("Kort tekstje");
}

void PrioTft::loop()
{
    // pBar.draw(cur_volume);
    // sTitle.loop();
}

void PrioTft::showLocalIp(const String &ip)
{
    tft.setCursor(0, 20); // Set cursor a top left of screen
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println("IP address: " + ip);
}

void PrioTft::setVolume(int _cur_volume)
{
    cur_volume = _cur_volume;
    pBar.draw(cur_volume);
}

void PrioTft::setTitle(const String &title)
{

    tft.setCursor(0, 45); // Set cursor a top left of screen
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if(title.length() < 40) {
 //       sTitle.setScrollerText(title);
        tft.println(title);
    } else {
        tft.println(title.substring(0, 40));
//        sTitle.setScrollerText(title.substring(0, 40));
    }
   // sTitle.setScrollerText(title);
}

void PrioTft::setLogo(const String &url)
{
    sLogo.Show(35, 100, url);
}