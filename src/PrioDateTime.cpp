#include "PrioDateTime.h"
#include <WiFi.h>
#include <time.h>

PrioDateTime::PrioDateTime(int clkPin, int datPin, int rstPin)
    : _threeWire(datPin, clkPin, rstPin), _rtc(_threeWire), // Initialiseer ThreeWire en RtcDS1302
      _clkPin(clkPin), _datPin(datPin), _rstPin(rstPin) {
    timeSynced = false;  // Standaard niet gesynchroniseerd
}

void PrioDateTime::begin() {
    // Initialiseer de RTC
    _rtc.Begin();

    // Controleer of de RTC werkt en een geldige tijd heeft
    if (!_rtc.IsDateTimeValid()) {
        Serial.println("⛔ RTC heeft geen geldige tijd! Synchroniseren met NTP...");
        syncTime(); // Synchroniseer de tijd met NTP als de RTC-tijd ongeldig is
    } else {
        Serial.println("✅ RTC heeft een geldige tijd.");
        timeSynced = true;
    }
}

void PrioDateTime::syncTime() {
    Serial.println("Synchroniseren met NTP-server...");

    // Stel de tijdzone in op GMT+1 (zonder zomertijd)
    configTime(3600, 0, "pool.ntp.org", "time.nist.gov");

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

        // Werk de RTC bij met de gesynchroniseerde tijd
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            RtcDateTime compiledDateTime(
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec
            );
            _rtc.SetDateTime(compiledDateTime);
            Serial.println("RTC bijgewerkt met NTP-tijd.");
        }
    }
}

char* PrioDateTime::getTime() {
    RtcDateTime now = _rtc.GetDateTime(); // Lees de tijd van de RTC

    snprintf(buffer, sizeof(buffer), "%02d:%02d", now.Hour(), now.Minute());
    return buffer;
}

char* PrioDateTime::getDate() {
    RtcDateTime now = _rtc.GetDateTime(); // Lees de datum van de RTC

    snprintf(buffer, sizeof(buffer), "%02d-%02d-%04d", now.Day(), now.Month(), now.Year());
    return buffer;
}

char* PrioDateTime::getDateTime() {
    RtcDateTime now = _rtc.GetDateTime(); // Lees datum en tijd van de RTC

    snprintf(buffer, sizeof(buffer), "%02d-%02d-%04d %02d:%02d",
             now.Day(), now.Month(), now.Year(), now.Hour(), now.Minute());
    return buffer;
}