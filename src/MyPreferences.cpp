#include "MyPreferences.h"

MyPreferences::MyPreferences(const char *namespaceName) : namespaceNameStr(namespaceName), initialized(false) {}

void MyPreferences::begin()
{
    if (!initialized)
    {
        if (!isNVSInitialized())
        {
            initializeNVS();
            delay(100);
            if (!isNVSInitialized())
            {
                Serial.println("Fout bij het initialiseren van NVS. Probeer opnieuw of herstart de ESP32.");
                // Eventueel ESP.restart() toevoegen
            }
        }

        if (preferences.begin(namespaceNameStr.c_str(), false))
        {
            Serial.println("Preferences voor " + String(namespaceNameStr) + " begonnen met succes!");
            initialized = true;
        }
        else
        {
            Serial.println("Fout bij het beginnen van Preferences voor " + String(namespaceNameStr) + "!");
        }
    }
}

MyPreferences::~MyPreferences()
{
    preferences.end();
    Serial.println("Preferences afgesloten!");
}

void MyPreferences::writeValue(const char *key, int value)
{
    preferences.putInt(key, value);
    Serial.println("Reading written value back");
    Serial.println(String(this->readValue(key, 0)));
}

int MyPreferences::readValue(const char *key, int defaultValue)
{
    return preferences.getInt(key, defaultValue);
}

void MyPreferences::writeString(const char *key, const char *value)
{
    Serial.println("Reading written string back");
    preferences.putString(key, value);
    Serial.println(this->readString(key, "not found"));
}

String MyPreferences::readString(const char *key, const char *defaultValue)
{
    return preferences.getString(key, defaultValue);
}

void MyPreferences::putUInt(const char *key, uint32_t value)
{
    preferences.putUInt(key, value);
}

uint32_t MyPreferences::getUInt(const char *key, uint32_t defaultValue)
{
    return preferences.getUInt(key, defaultValue);
}

bool MyPreferences::isNVSInitialized()
{
    esp_err_t err = nvs_flash_init();
    return err == ESP_OK;
}

void MyPreferences::remove(const char *key)
{
    preferences.remove(key);
}

void MyPreferences::initializeNVS()
{
    // Wis de NVS-partitie
    esp_err_t err = nvs_flash_erase();
    if (err == ESP_OK)
    {
        Serial.println("NVS-partitie gewist.");
    }
    else
    {
        Serial.println("Fout bij het wissen van de NVS-partitie.");
    }

    // Initialiseer de NVS-partitie opnieuw
    err = nvs_flash_init();
    if (err == ESP_OK)
    {
        Serial.println("NVS-partitie geïnitialiseerd.");
    }
    else
    {
        Serial.println("Fout bij het initialiseren van de NVS-partitie.");
    }
}
