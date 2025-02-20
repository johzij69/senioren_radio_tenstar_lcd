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
    prepLine(2);
    tft.println("IP address: " + ip);
}

void PrioTft::setVolume(int _cur_volume)
{
    cur_volume = _cur_volume;
    pBar.draw(cur_volume);
}

void PrioTft::setTitle(const String &title)
{
    prepLine(3);
    if (title.length() < 32)
    {
        tft.println(title);
    }
    else
    {
        tft.println(title.substring(0, 32));
    }
     // When chneg streamTitle, we need to wipe the streamTitle name
     clearLine(4);
}

void PrioTft::setStreamTitle(const String &streamTitle)
{
    prepLine(4);
    if (streamTitle.length() < 32)
    {
        tft.println(streamTitle);
    }
    else
    {
        tft.println(streamTitle.substring(0, 32));
    }
   
}

void PrioTft::prepLine(int lineNumber)
{
    int line = 25 * (lineNumber-1); // We will start counting at 1
    clearLine(lineNumber);
    tft.setCursor(0, line);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
}
void PrioTft::clearLine(int lineNumber)
{
    int line = 25 * (lineNumber-1);
    tft.fillRect(0, line, tft.width() - pBar.width_set, tft.fontHeight(), TFT_BLACK);
}

void PrioTft::setLogo(const String &url)
{
    sLogo.Show(35, 100, url);
}