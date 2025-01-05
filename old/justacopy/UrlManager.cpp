#include "UrlManager.h"
#include <Arduino.h>

UrlManager::UrlManager(MyPreferences &prefs) : myPreferences(prefs)
{
    // Laad URL's vanuit Preferences bij het initialiseren
 //   urlCount = myPreferences.getUInt("url_count", 0);
    streamCount = myPreferences.getUInt("stream_count", 0);
    default_logo = "https://img.prio-ict.nl/api/images/webradio-default.jpg";
}

void UrlManager::begin()
{
  //  this->loadUrls();
    this->loadStreams();
}

// void UrlManager::loadUrls()
// {
//     urlCount = myPreferences.getUInt("url_count", 0);
//     if (urlCount < 1)
//     {
//         return;
//     }
//     urls.clear();
//     for (uint32_t i = 0; i < urlCount; ++i)
//     {
//         String key = "url" + String(i);
//         String logo_key = "logo_url" + String(i);

//         String url = myPreferences.readString(key.c_str(), "");
//         String logo_url = myPreferences.readString(logo_key.c_str(), default_logo.c_str());

//         urls.push_back(url);
//         logo_urls.push_back(logo_url);
//     }
// }

void UrlManager::loadStreams()
{

    streamCount = myPreferences.getUInt("stream_count", 0);
    Serial.println("loading streams");
    Serial.println("stream count is:" + String(streamCount));

    for (int i = 0; i < streamCount; i++)
    {
        String keyId = "Id" + String(i);
        String keyName = "name" + String(i);
        String keyUrl = "url" + String(i);
        String keyLogo = "logo" + String(i);
        Streams[i].id = myPreferences.getUInt(keyId.c_str(), i);
        Streams[i].name = myPreferences.readString(keyName.c_str(), "");
        Streams[i].url = myPreferences.readString(keyUrl.c_str(), "");
        Streams[i].logo = myPreferences.readString(keyLogo.c_str(), "");
    }
}


void UrlManager::addStream(uint8_t *data)
{

    // Max # of streams is 40
    if (streamCount == 39)
    {
        return;
    }

    JsonDocument doc;
    char *jsonString = reinterpret_cast<char *>(data);
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error)
    {
        Serial.print("deserializeJson() returned ");
        Serial.println(error.c_str());
        return;
    }

    JsonObject json_settings = doc["json_settings"];

    const char *json_settings_name = json_settings["name"];
    const char *json_settings_url = json_settings["url"];
    const char *json_settings_logo = json_settings["logo"];

    Streams[streamCount].id = streamCount;
    Streams[streamCount].name = json_settings_name;
    Streams[streamCount].url = json_settings_url;
    Streams[streamCount].logo = json_settings_logo;

    Serial.println("addstream:");
    Serial.println(String(Streams[streamCount].id));
    Serial.println(String(Streams[streamCount].name));
    Serial.println(String(Streams[streamCount].url));
    Serial.println(String(Streams[streamCount].logo));

    this->streamCount++;
    this->saveToPreferences();
}

void UrlManager::saveStreams(uint8_t *data)
{

    int index = 0; // Teller voor de index
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data);

    if (error)
    {
        Serial.print("deserializeJson() returned ");
        Serial.println(error.c_str());
        return ;
    }

    for (JsonPair item : doc.as<JsonObject>())
    {
        const char *item_key = item.key().c_str(); // "container-0", "container-1", "container-2", ...

        Serial.println("saved:" + String(item_key));

        index = atoi(item_key + strlen("container-"));


        const char *value_name = item.value()["name"]; 
        const char *value_url = item.value()["url"];
        const char *value_logo = item.value()["logo"];

        Streams[index].id = item.value()["id"];
        Streams[index].name = value_name;
        Streams[index].url = value_url;
        Streams[index].logo = value_logo;

        Serial.print("saved:");
        Serial.println(String(Streams[index].id));
        Serial.println(String(Streams[index].name));
        Serial.println(String(Streams[index].url));
        Serial.println(String(Streams[index].logo));
   
    }

    this->saveToPreferences();
}

void UrlManager::deleteStream(int index)
{

    if (index < 0 || index > streamCount - 1)
    {
        Serial.println("Ongeldige index");
        return;
    }

    // Schuif alle items na de verwijderde index één plaats naar links
    for (int i = index; i < 39; i++)
    {
        Streams[i] = Streams[i + 1];
    }

    // Verwijder het laatste item door het te resetten naar de standaardwaarden
    StreamItem defaultItem;
    Streams[39] = defaultItem;

    this->streamCount--;
    this->saveToPreferences();
}


void UrlManager::saveToPreferences()
{

    Serial.println("saving to prefs");
    Serial.println(String(streamCount));

    for (int i = 0; i < streamCount; i++)
    {

    Serial.println(String(streamCount));
        String keyId = "Id" + String(i);
        String keyName = "name" + String(i);
        String keyUrl = "url" + String(i);
        String keyLogo = "logo" + String(i);

        Serial.println(Streams[i].id);
        Serial.println(Streams[i].name);
        Serial.println(Streams[i].url);
        Serial.println(Streams[i].logo);

        myPreferences.putUInt(keyId.c_str(), Streams[i].id);
        myPreferences.writeString(keyName.c_str(), Streams[i].name.c_str());
        myPreferences.writeString(keyUrl.c_str(), Streams[i].url.c_str());
        myPreferences.writeString(keyLogo.c_str(), Streams[i].logo.c_str());
    }
    myPreferences.putUInt("stream_count", streamCount);
}

// void UrlManager::saveUrls()
// {
//     urlCount = urls.size();
//     if (urlCount < 1)
//     {
//         return;
//     }
//     for (uint32_t i = 0; i < urlCount; ++i)
//     {
//         String key = "url" + String(i);
//         String logo_key = "logo_url" + String(i);

//         myPreferences.writeString(key.c_str(), urls[i].c_str());
//         myPreferences.writeString(logo_key.c_str(), logo_urls[i].c_str());
//     }
//     myPreferences.putUInt("url_count", urls.size());
// }

void UrlManager::readAndPrintValue(const char *key)
{
    // Lees de waarde vanuit UrlManager
    int value = myPreferences.readValue(key, 0);
    Serial.println("Waarde gelezen vanuit UrlManager: " + String(value));
}
// void UrlManager::addUrl(const char *url)
// {
//     this->addUrl(url, default_logo.c_str());
// }
// void UrlManager::addUrl(const char *url, const char *logo_url)
// {
//     Serial.println("Toevogen url: " + String(url));

//     // Controleer of de URL al bestaat
//     if (std::find(urls.begin(), urls.end(), url) != urls.end())
//     {
//         Serial.println("URL bestaat al, wordt niet toegevoegd: " + String(url));
//         return;
//     }

//     // Voeg de URL toe aan de lijst en Preferences
//     urls.push_back(url);
//     String key = "url" + String(urls.size() - 1);
//     myPreferences.writeString(key.c_str(), url);
//     myPreferences.putUInt("url_count", urls.size());
//     Serial.println("URL toegevoegd: " + String(url));

//     this->loadUrls();
// }

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

// void UrlManager::PrinturlsFromPrev()
// {
//     urlCount = myPreferences.getUInt("url_count", 0);
//     Serial.print("url_count:");
//     Serial.println(urlCount);

//     for (uint32_t i = 0; i < urlCount; ++i)
//     {
//         String key = "url" + String(i);
//         String url = myPreferences.readString(key.c_str(), "");
//         Serial.println(url);
//     }
// }

// void UrlManager::deleteUrl(int index)
// {

//     if (index < urls.size())
//     {
//         Serial.println("Deleting: " + String(urls[index]));
//         urls.erase(urls.begin() + index);

//         String key = "url" + String(index);
//         if (myPreferences.remove(key.c_str()))
//         {
//             Serial.println(key + ": verwijdert.");
//         }

//         myPreferences.putUInt("url_count", urls.size());
//         Serial.println("kom ik hier");
//         Serial.println("URL verwijderd: " + String(urls[index]));
//         Serial.println("kom ik hier2");
//         this->loadUrls();
//     }
// }