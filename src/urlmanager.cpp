#include "UrlManager.h"
#include <Arduino.h>



UrlManager::UrlManager(MyPreferences &prefs) : myPreferences(prefs)
{
    // Laad URL's vanuit Preferences bij het initialiseren
    urlCount = myPreferences.getUInt("url_count", 0);
    default_logo = "https://img.prio-ict.nl/api/images/webradio-default.jpg";
}

void UrlManager::begin()
{
    this->loadUrls();
}

void UrlManager::loadUrls()
{
    urlCount = myPreferences.getUInt("url_count", 0);
    if (urlCount < 1)
    {
        return;
    }
    urls.clear();
    for (uint32_t i = 0; i < urlCount; ++i)
    {
        String key = "url" + String(i);
        String logo_key = "logo_url" + String(i);
        
        String url = myPreferences.readString(key.c_str(), "");
        String logo_url = myPreferences.readString(logo_key.c_str(), default_logo.c_str());
        
        urls.push_back(url);
        logo_urls.push_back(logo_url);
    }
}

void UrlManager::saveUrls()
{
    urlCount = urls.size();
    if (urlCount < 1)
    {
        return;
    }
    for (uint32_t i = 0; i < urlCount; ++i)
    {
        String key = "url" + String(i);
        String logo_key = "logo_url" + String(i);

        myPreferences.writeString(key.c_str(), urls[i].c_str());
        myPreferences.writeString(logo_key.c_str(), logo_urls[i].c_str());
    }
    myPreferences.putUInt("url_count", urls.size());
}

void UrlManager::readAndPrintValue(const char *key)
{
    // Lees de waarde vanuit UrlManager
    int value = myPreferences.readValue(key, 0);
    Serial.println("Waarde gelezen vanuit UrlManager: " + String(value));
}
void UrlManager::addUrl(const char *url)
{
    this->addUrl(url,default_logo.c_str());
}    
void UrlManager::addUrl(const char *url, const char *logo_url)
{
    Serial.println("Toevogen url: " + String(url));

    // Controleer of de URL al bestaat
    if (std::find(urls.begin(), urls.end(), url) != urls.end())
    {
        Serial.println("URL bestaat al, wordt niet toegevoegd: " + String(url));
        return;
    }

    // Voeg de URL toe aan de lijst en Preferences
    urls.push_back(url);
    String key = "url" + String(urls.size() - 1);
    myPreferences.writeString(key.c_str(), url);
    myPreferences.putUInt("url_count", urls.size());
    Serial.println("URL toegevoegd: " + String(url));

    this->loadUrls();
}

void UrlManager::printAllUrls()
{
    Serial.println("Alle URLs:");
    for (const auto &url : urls)
    {
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

    String result = "<form>";

    for (size_t i = 0; i < urls.size(); i++)
    {
        Serial.println(urls[i]);
        result += "<div id=\"" + String(i) + "\"><p>URL " + String(i) + ": " + urls[i] + "<p><div>";
    }

    return result;
}

void UrlManager::PrinturlsFromPrev()
{
    urlCount = myPreferences.getUInt("url_count", 0);
    Serial.print("url_count:");
    Serial.println(urlCount);

    for (uint32_t i = 0; i < urlCount; ++i)
    {
        String key = "url" + String(i);
        String url = myPreferences.readString(key.c_str(), "");
        Serial.println(url);
    }
}

void UrlManager::deleteUrl(int index)
{

    if (index < urls.size())
    {
        Serial.println("Deleting: " + String(urls[index]));
        urls.erase(urls.begin() + index);

        String key = "url" + String(index);
        if (myPreferences.remove(key.c_str())) {
            Serial.println(key+ ": verwijdert.");
        }

        myPreferences.putUInt("url_count", urls.size());
Serial.println("kom ik hier");
        Serial.println("URL verwijderd: " + String(urls[index]));
Serial.println("kom ik hier2");
        this->loadUrls();
    }
}