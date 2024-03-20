#include "UrlManager.h"

UrlManager::UrlManager(const char *namespaceName) : preferences(), namespaceName(namespaceName)
{
    preferences.begin(namespaceName, false);

    Serial.println("namewspaceName");
    Serial.println(namespaceName);

    // Load saved URLs on startup
    for (int i = 0; i < preferences.getUInt("url_count", 0); i++)
    {
        String key = "url" + String(i);
        const char *storedURL = preferences.getString(key.c_str(), "").c_str(); // Convert to const char*
        if (storedURL != nullptr && storedURL[0] != '\0')
        {
            urls.push_back(storedURL);
        }
        else
        {
            break; // Stop at the first empty key
        }
    }
}

void UrlManager::addUrl(const char *url)
{
    // Controleer of de URL al bestaat
    if (std::find(urls.begin(), urls.end(), url) != urls.end())
    {
        Serial.println("URL bestaat al, wordt niet toegevoegd: " + String(url));
        return;
    }

    // Voeg de URL toe aan de lijst en Preferences
    urls.push_back(url);
    String key = "url" + String(urls.size() - 1);
    preferences.putString(key.c_str(), url);
    preferences.putUInt("url_count", urls.size());
    Serial.println("URL toegevoegd: " + String(url));
}

void UrlManager::removeUrl(const char *url)
{
    auto it = std::find(urls.begin(), urls.end(), url);
    if (it != urls.end())
    {
        size_t index = std::distance(urls.begin(), it);
        urls.erase(it);
        preferences.remove(("url" + String(index)).c_str());
        preferences.putUInt("url_count", urls.size());
        Serial.println("URL verwijderd: " + String(url));
    }
    else
    {
        Serial.println("URL niet gevonden om te verwijderen.");
    }
}

void UrlManager::printUrls()
{
    for (size_t i = 0; i < urls.size(); i++)
    {
        Serial.println("URL " + String(i) + ": " + urls[i]);
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
