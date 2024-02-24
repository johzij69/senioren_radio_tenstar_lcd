#include "UrlManager.h"
#include <Arduino.h>

UrlManager::UrlManager(MyPreferences& prefs) : myPreferences(prefs) {
    // Laad URL's vanuit Preferences bij het initialiseren
    uint32_t urlCount = myPreferences.readValue("url_count", 0);
    for (uint32_t i = 0; i < urlCount; ++i) {
        String key = "url" + String(i);
        String url = myPreferences.readString(key.c_str(), "");
        urls.push_back(url);
    }
}

void UrlManager::readAndPrintValue(const char* key) {
    // Lees de waarde vanuit UrlManager
    int value = myPreferences.readValue(key, 0);
    Serial.println("Waarde gelezen vanuit UrlManager: " + String(value));
}

void UrlManager::addUrl(const char* url) {
     Serial.println("Toevogen url: " + String(url));
    
    // Controleer of de URL al bestaat
    if (std::find(urls.begin(), urls.end(), url) != urls.end()) {
        Serial.println("URL bestaat al, wordt niet toegevoegd: " + String(url));
        return;
    }

    // Voeg de URL toe aan de lijst en Preferences
    urls.push_back(url);
    String key = "url" + String(urls.size() - 1);
    myPreferences.writeString(key.c_str(), url);
    myPreferences.putUInt("url_count", urls.size());
    Serial.println("URL toegevoegd: " + String(url));
}

void UrlManager::printAllUrls() {
    Serial.println("Alle URLs:");
    for (const auto& url : urls) {
        Serial.println(url);
    }
}

const char *UrlManager::getUrlAtIndex(int index)
{
    if (index >= 0 && static_cast<size_t>(index) < urls.size())
    {
        return urls[index].c_str();
    }
    else
    {
        return nullptr;
    }
}

String UrlManager::CreateDivUrls()
{

    String result = "";

    for (size_t i = 0; i < urls.size(); i++)
    {
        Serial.println(urls[i]);
        result += "<div><p>URL " + String(i) + ": " + urls[i] + "<p><div>";
    }

    return result;
}




