#include "PrioTft.h"

PrioTft::PrioTft() : tft(), pBar(&tft), sLogo(&tft), max_volume(30), last_volume(10), cur_volume(0), isInitialized(false), sTitle(tft, 20, 60, 100, 450, FM12)
{
    // Constructor body (indien nodig)
}

void PrioTft::begin()
{
    tft.init();         // Initialiseer het scherm
    tft.setRotation(3); // Pas rotatie aan indien nodig
    init();

    // sTitle.begin();
    //   sTitle.setScrollerText("Dit is een lange tekst die zal scrollen, zodat je de hele tekst kunt lezen.");
    //  sTitle.setScrollerText("Kort tekstje");
    Serial.println("TFT scherm is geïnitialiseerd.");
}

void PrioTft::init()
{
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(2, 2, 4);
    tft.setTextColor(TFT_WHITE);
    tft.println("PRIO-WEBRADIO");
    pBar.begin(450, 0, 30, 320, TFT_LIGHTGREY, TFT_BLACK, max_volume);
    pBar.draw(last_volume);
    sLogo.begin();
    isInitialized = true; // Scherm is geïnitialiseerd
    cur_volume = last_volume;
    drawParaLine("Titel: ", 251);
}

void PrioTft::showStandbyState()
{
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(2, 2, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println("PRIO-WEBRADIO");
    tft.setTextFont(10);
    tft.setTextSize(1);
    tft.setCursor(10, 90);
    tft.println("Standby mode");
    tft.setTextFont(4);
    tft.setTextSize(1);
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
}

void PrioTft::setTitle(const String &title)
{
    prepLine(10);
    String truncatedTitle = truncateStringToFit(title, tft.width() - (pBar.width_set + kantline));
    tft.println(truncatedTitle);
    // When Title changed, we need to wipe the streamTitle name
    clearLine(12);
}

void PrioTft::setStreamTitle(const String &streamTitle)
{
    prepLine(12);
    String truncatedStreamTitle = truncateStringToFit(streamTitle, tft.width() - (pBar.width_set + kantline));
    tft.println(truncatedStreamTitle);
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
    tft.fillRect(0, line, tft.width() - pBar.width_set, tft.fontHeight() + 2, TFT_GREYBLUE);
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

// void PrioTft::showStandbyTime(const String &time)
// {

//     File root = LittleFS.open("/fonts/");
//     File file = root.openNextFile();
//     while (file)
//     {
//         Serial.println(file.name());
//         file = root.openNextFile();

//     }

void PrioTft::showStandbyTime(const String &time)
{
    String fontPath = "/fonts/Audiowide_Regular64.vlw";
    
    Serial.println("=== Font Loading Debug ===");
    Serial.printf("Font path: %s\n", fontPath.c_str());
    Serial.printf("File exists: %s\n", LittleFS.exists(fontPath) ? "YES" : "NO");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    
    if (LittleFS.exists(fontPath)) {
        File f = LittleFS.open(fontPath, "r");
        if (f) {
            Serial.printf("File size: %d bytes\n", f.size());
            f.close();
        }
    }
    
    tft.fillScreen(TFT_BLACK);
    tft.unloadFont(); // Clear any existing font
    
    Serial.println("Attempting to load font...");
    tft.loadFont(fontPath, LittleFS);
    Serial.printf("Free heap after load: %d bytes\n", ESP.getFreeHeap());
    
    // Test of font geladen is door te tekenen
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(time, tft.width() / 2, tft.height() / 2);
    
    Serial.println("Font load attempt completed");
}

String PrioTft::truncateStringToFit(const String &text, int maxWidth)
{
    int textWidth = tft.textWidth(text);
    if (textWidth <= maxWidth)
    {
        return text;
    }

    String truncatedText = text;
    while (textWidth > maxWidth && truncatedText.length() > 0)
    {
        truncatedText.remove(truncatedText.length() - 1);
        textWidth = tft.textWidth(truncatedText);
    }

    return truncatedText;
}

void PrioTft::showLoadingState()
{
    tft.setTextFont(8);
    tft.setTextSize(1);
    tft.fillRect(10, 90, tft.width() - 10, tft.fontHeight(), TFT_BLACK);
    tft.setCursor(10, 90);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println("Loading....");
    tft.setTextFont(4);
    tft.setTextSize(1);
}