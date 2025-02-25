// see -> https://doc-tft-espi.readthedocs.io/
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
    tft.setCursor(2, 2, 4);
    tft.setTextColor(TFT_WHITE);
    tft.println("PRIO-WEBRADIO");
    pBar.begin(450, 0, 30, 320, TFT_LIGHTGREY, TFT_BLACK, max_volume);
    pBar.draw(last_volume);
    sLogo.begin();
    sLogo.Show(kantline, 100, "https://img.prio-ict.nl/api/images/webradio-default.jpg");
    isInitialized = true; // Scherm is geïnitialiseerd
    cur_volume = last_volume;
    drawParaLine("Titel: ", 251);
    
    //sTitle.begin();
    //  sTitle.setScrollerText("Dit is een lange tekst die zal scrollen, zodat je de hele tekst kunt lezen.");
    // sTitle.setScrollerText("Kort tekstje");
}

void PrioTft::loop()
{
    // pBar.draw(cur_volume);
    // sTitle.loop();
}

void PrioTft::showLocalIp(const String &ip)
{
    clearGreenLine(1);
    tft.setCursor(2, 2, 4);
    tft.setTextColor(TFT_WHITE, TFT_GREYBLUE);
    tft.println("PRIO-WEBRADIO: " + ip);
}

void PrioTft::setVolume(int _cur_volume)
{
    cur_volume = _cur_volume;
    pBar.draw(cur_volume);
}

void PrioTft::drawParaLine(const String &text, int startY)
{

    tft.drawLine(10, startY, tft.width() - (pBar.width_set), startY, TFT_GREYBLUE);
    // for (int i = 1; i < 18; i++)
    // {
    //     tft.drawLine(10 + i, startY + i, 120 - i, startY + i, TFT_GREYBLUE);
    // }

    // tft.setTextFont(2);
    // tft.setTextSize(1);
    // tft.setCursor(kantline, startY+1);
    // tft.setTextColor(TFT_BLACK, TFT_GREYBLUE);
    // tft.println(text);
    // tft.setTextFont(4);
    // tft.setTextColor(TFT_WHITE,TFT_BLACK );

}

void PrioTft::setTitle(const String &title)
{
    prepLine(10);
    if (title.length() < 32)
    {
        tft.println(title);
    }
    else
    {
        tft.println(title.substring(0, 32));
    }
    // When Title changed, we need to wipe the streamTitle name
    clearLine(12);
}

void PrioTft::setStreamTitle(const String &streamTitle)
{
    prepLine(12);
    if (streamTitle.length() < 34)
    {
        tft.println(streamTitle);
    }
    else
    {
        tft.println(streamTitle.substring(0, 34));
    }
}

void PrioTft::prepLine(int lineNumber)
{
    int line = 25 * (lineNumber - 1); // We will start counting at 1
    clearLine(lineNumber);
    tft.setCursor(kantline, line);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void PrioTft::clearLine(int lineNumber)
{
    int line = 25 * (lineNumber - 1);
    tft.fillRect(kantline, line, tft.width() - (pBar.width_set + kantline), tft.fontHeight(), TFT_BLACK);
}

void PrioTft::clearLogoLine(int lineNumber)
{
    int line = 25 * (lineNumber - 1);
    tft.fillRect(200, line, tft.width() - (pBar.width_set + 200), tft.fontHeight(), TFT_BLACK);
}
void PrioTft::clearGreenLine(int lineNumber)
{
    int line = 25 * (lineNumber - 1);
    tft.fillRect(0, line, tft.width() - pBar.width_set, tft.fontHeight()+2, TFT_GREYBLUE);
}

void PrioTft::setLogo(const String &url)
{
    sLogo.Show(kantline, 50, url);
}

void PrioTft::showTime(const String &time)
{

    tft.setTextFont(8);
    tft.setTextSize(1);
    tft.fillRect(185, 90, tft.width() - (pBar.width_set + 200), tft.fontHeight(), TFT_BLACK);
    tft.setCursor(185, 90);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println(time);
    tft.setTextFont(4);
    tft.setTextSize(1);
}