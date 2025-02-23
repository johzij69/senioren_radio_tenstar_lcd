#include "PrioDateTime.h"
#include <WiFi.h>
#include <time.h>

PrioDateTime::PrioDateTime() {
    timeSynced = false;  // Standaard niet gesynchroniseerd
}

void PrioDateTime::begin() {
    syncTime();
}

void PrioDateTime::syncTime() {
    Serial.println("Synchroniseren met NTP-server...");

    configTime(3600, 3600, "pool.ntp.org", "time.nist.gov"); // UTC+1 (wintertijd), +2 in zomer

    // Wachten op tijdsynchronisatie (max 20 sec)
    int timeout = 20;
    time_t now = time(nullptr);
    while (now < 1000000000 && timeout > 0) {
        delay(1000);
        now = time(nullptr);
        Serial.print(".");
        timeout--;
    }

    if (timeout == 0) {
        Serial.println("\n⛔ Tijd synchronisatie mislukt!");
        timeSynced = false;
    } else {
        Serial.println("\n✅ Tijd gesynchroniseerd!");
        timeSynced = true;
    }
}

char* PrioDateTime::getTime() {
    if (!timeSynced) {
        strcpy(buffer, "00:00");
        return buffer;
    }
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        strcpy(buffer, "00:00");
        return buffer;
    }
    
    snprintf(buffer, sizeof(buffer), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    return buffer;
}

char* PrioDateTime::getDate() {
    if (!timeSynced) {
        strcpy(buffer, "00-00-0000");
        return buffer;
    }
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        strcpy(buffer, "00-00-0000");
        return buffer;
    }
    
    snprintf(buffer, sizeof(buffer), "%02d-%02d-%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
    return buffer;
}

char* PrioDateTime::getDateTime() {
    if (!timeSynced) {
        strcpy(buffer, "00-00-0000 00:00");
        return buffer;
    }
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        strcpy(buffer, "00-00-0000 00:00");
        return buffer;
    }
    
    snprintf(buffer, sizeof(buffer), "%02d-%02d-%04d %02d:%02d",
             timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
             timeinfo.tm_hour, timeinfo.tm_min);
    return buffer;
}
