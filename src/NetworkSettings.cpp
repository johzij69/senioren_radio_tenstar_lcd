#include "NetworkSettings.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

namespace
{
struct NetworkCommand
{
    char bssid[18];
    uint32_t timeoutSeconds;
};

QueueHandle_t commands = xQueueCreate(1, sizeof(NetworkCommand));
SemaphoreHandle_t stateMutex = xSemaphoreCreateMutex();
String preferredSsid;
String preferredPassword;
String preferredBssid;
uint32_t timeoutSeconds = 30;
JsonDocument scanResults;
String scanError;
String connectionMessage;
bool scanning = false;
bool connecting = false;
bool fallback = false;
uint32_t connectStarted = 0;
uint32_t applyAfter = 0;

bool savePreference(const String &ssid, const String &password, const String &bssid, uint32_t seconds)
{
    JsonDocument saved;
    saved["ssid"] = ssid;
    saved["password"] = password;
    saved["bssid"] = bssid;
    saved["timeout"] = seconds;
    String config;
    serializeJson(saved, config);
    Preferences storage;
    if (!storage.begin("radioNetwork", false)) return false;
    bool persisted = storage.putString("accessPoint", config) == config.length();
    storage.end();
    return persisted;
}

bool parseBssid(const String &text, uint8_t *address)
{
    if (text.length() != 17) return false;
    for (size_t index = 0; index < 6; ++index)
    {
        size_t offset = index * 3;
        if (!isxdigit(text[offset]) || !isxdigit(text[offset + 1]) ||
            (index < 5 && text[offset + 2] != ':')) return false;
        address[index] = strtoul(text.substring(offset, offset + 2).c_str(), nullptr, 16);
    }
    return true;
}

void startConnection(bool usePreferred)
{
    uint8_t address[6];
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    if (usePreferred && parseBssid(preferredBssid, address))
    {
        WiFi.begin(preferredSsid.c_str(), preferredPassword.c_str(), 0, address);
        connectionMessage = "Verbinden met voorkeursaccesspoint";
    }
    else
    {
        WiFi.begin(preferredSsid.c_str(), preferredPassword.c_str(), 0, nullptr);
        connectionMessage = "Terugval naar een ander accesspoint van hetzelfde netwerk";
    }
    fallback = !usePreferred;
    connecting = true;
    connectStarted = millis();
}
}

bool NetworkSettings::connectPreferred()
{
    Preferences storage;
    if (!storage.begin("radioNetwork", false)) return false;
    String config = storage.getString("accessPoint", "");
    storage.end();
    JsonDocument saved;
    if (deserializeJson(saved, config)) return false;
    preferredSsid = saved["ssid"].as<String>();
    preferredPassword = saved["password"].as<String>();
    preferredBssid = saved["bssid"].as<String>();
    timeoutSeconds = saved["timeout"] | 30U;
    uint8_t address[6];
    if (preferredSsid.isEmpty() || !parseBssid(preferredBssid, address) ||
        timeoutSeconds < 5 || timeoutSeconds > 300)
    {
        preferredBssid = "";
        return false;
    }
    WiFi.mode(WIFI_STA);
    startConnection(true);
    while (WiFi.status() != WL_CONNECTED && millis() - connectStarted < timeoutSeconds * 1000UL)
    {
        delay(50);
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        startConnection(false);
        while (WiFi.status() != WL_CONNECTED && millis() - connectStarted < timeoutSeconds * 1000UL)
        {
            delay(50);
        }
    }
    connecting = false;
    bool connected = WiFi.status() == WL_CONNECTED;
    if (!connected) WiFi.setAutoReconnect(true);
    return connected;
}

void NetworkSettings::connectionReady()
{
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) != pdTRUE) return;
    if (!preferredBssid.isEmpty() && WiFi.SSID() != preferredSsid)
    {
        Preferences storage;
        if (storage.begin("radioNetwork", false))
        {
            storage.remove("accessPoint");
            storage.end();
        }
        preferredBssid = "";
        preferredPassword = "";
    }
    if (!preferredBssid.isEmpty())
    {
        String password = WiFi.psk();
        if (password != preferredPassword)
        {
            preferredPassword = password;
            if (!savePreference(preferredSsid, password, preferredBssid, timeoutSeconds))
                Serial.println("Bijwerken van wifi-inloggegevens mislukt");
        }
        WiFi.setAutoReconnect(false);
        fallback = !WiFi.BSSIDstr().equalsIgnoreCase(preferredBssid);
        connectionMessage = fallback ? "Verbonden via terugvalaccesspoint" : "Verbonden met voorkeursaccesspoint";
    }
    else connectionMessage = "Verbonden; geen voorkeursaccesspoint ingesteld";
    connecting = false;
    xSemaphoreGive(stateMutex);
}

bool NetworkSettings::requestScan()
{
    NetworkCommand command = {};
    return commands && xQueueSend(commands, &command, 0) == pdTRUE;
}

bool NetworkSettings::selectAccessPoint(const String &bssid, uint32_t seconds)
{
    uint8_t address[6];
    if (!commands || seconds < 5 || seconds > 300 || !parseBssid(bssid, address) ||
        xSemaphoreTake(stateMutex, 0) != pdTRUE) return false;
    bool found = false;
    for (JsonObject entry : scanResults["networks"].as<JsonArray>())
    {
        if (entry["bssid"].as<String>().equalsIgnoreCase(bssid) &&
            entry["ssid"].as<String>() == WiFi.SSID()) found = true;
    }
    NetworkCommand command = {};
    bssid.toCharArray(command.bssid, sizeof(command.bssid));
    command.timeoutSeconds = seconds;
    bool queued = found && !scanning && xQueueSend(commands, &command, 0) == pdTRUE;
    xSemaphoreGive(stateMutex);
    return queued;
}

bool NetworkSettings::getStatus(String &json)
{
    if (xSemaphoreTake(stateMutex, 0) != pdTRUE) return false;
    JsonDocument status;
    status["ssid"] = WiFi.SSID();
    status["bssid"] = WiFi.status() == WL_CONNECTED ? WiFi.BSSIDstr() : String("");
    status["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    status["preferredBssid"] = preferredBssid;
    status["timeout"] = timeoutSeconds;
    status["scanning"] = scanning;
    status["error"] = scanError;
    status["message"] = connectionMessage;
    status["networks"] = scanResults["networks"];
    serializeJson(status, json);
    xSemaphoreGive(stateMutex);
    return true;
}

void NetworkSettings::poll()
{
    if (xSemaphoreTake(stateMutex, 0) != pdTRUE) return;
    NetworkCommand command;
    if (!applyAfter && commands && xQueueReceive(commands, &command, 0) == pdTRUE)
    {
        if (command.bssid[0] == '\0')
        {
            if (!scanning && !connecting && WiFi.status() == WL_CONNECTED)
            {
                WiFi.scanDelete();
                scanError = "";
                int result = WiFi.scanNetworks(true, true, false, 120);
                scanning = result == WIFI_SCAN_RUNNING || result >= 0;
                if (!scanning) scanError = "Wifi-scan kon niet starten";
            }
        }
        else if (!scanning && WiFi.status() == WL_CONNECTED)
        {
            String ssid = WiFi.SSID();
            String password = WiFi.psk();
            bool persisted = savePreference(ssid, password, command.bssid, command.timeoutSeconds);
            if (persisted)
            {
                preferredSsid = ssid;
                preferredPassword = password;
                preferredBssid = command.bssid;
                timeoutSeconds = command.timeoutSeconds;
                applyAfter = millis();
                connectionMessage = "Voorkeursaccesspoint opgeslagen; verbinding wordt gewijzigd";
            }
            else connectionMessage = "Opslaan van netwerkvoorkeur mislukt";
        }
    }
    if (scanning)
    {
        int count = WiFi.scanComplete();
        if (count != WIFI_SCAN_RUNNING)
        {
            scanResults.clear();
            JsonArray networks = scanResults["networks"].to<JsonArray>();
            for (int index = 0; index < count && index < 64; ++index)
            {
                JsonObject entry = networks.add<JsonObject>();
                entry["ssid"] = WiFi.SSID(index);
                entry["bssid"] = WiFi.BSSIDstr(index);
                entry["rssi"] = WiFi.RSSI(index);
                entry["channel"] = WiFi.channel(index);
            }
            if (count < 0) scanError = "Wifi-scan mislukt; probeer opnieuw";
            scanning = false;
            WiFi.scanDelete();
        }
    }
    if (applyAfter && millis() - applyAfter >= 2000)
    {
        applyAfter = 0;
        startConnection(true);
    }
    if (!preferredBssid.isEmpty() && !applyAfter && !scanning)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            if (connecting) connectionMessage = fallback ? "Verbonden via terugvalaccesspoint" : "Verbonden met voorkeursaccesspoint";
            connecting = false;
        }
        else if (!connecting) startConnection(true);
        else if (millis() - connectStarted >= timeoutSeconds * 1000UL) startConnection(false);
    }
    xSemaphoreGive(stateMutex);
}

void NetworkSettings::reset()
{
    if (stateMutex) xSemaphoreTake(stateMutex, portMAX_DELAY);
    Preferences storage;
    bool cleared = storage.begin("radioNetwork", false);
    if (cleared)
    {
        cleared = storage.clear();
        storage.end();
    }
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);
    bool wifiCleared = esp_wifi_set_storage(WIFI_STORAGE_FLASH) == ESP_OK;
    if (wifiCleared) wifiCleared = WiFi.disconnect(false, true);
    if (!cleared || !wifiCleared)
    {
        Serial.println("Netwerkreset mislukt; probeer opnieuw");
        WiFi.setAutoReconnect(true);
        if (stateMutex) xSemaphoreGive(stateMutex);
        return;
    }
    ESP.restart();
}