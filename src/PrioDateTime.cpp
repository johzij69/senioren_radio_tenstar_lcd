#include "PrioDateTime.h"

PrioDateTime::PrioDateTime(int clkPin, int datPin, int rstPin)
    : _threeWire(datPin, clkPin, rstPin), _rtc(_threeWire)
{                                    // Initialiseer ThreeWire en RtcDS1302
    timeSynced = false;              // Standaard niet gesynchroniseerd
    _lastSyncTime = 0;               // Laatste synchronisatietijd (in milliseconden)
    _syncInterval = 1 * 3600 * 1000; // Synchroniseer elke 1 uur (1 uur * 3600 seconden * 1000 ms)
}

void PrioDateTime::begin()
{
    // Initialiseer de RTC
    _rtc.Begin();

    // Controleer of de RTC werkt en een geldige tijd heeft
    if (!_rtc.IsDateTimeValid())
    {
        Serial.println("⛔ RTC heeft geen geldige tijd! Synchroniseren met NTP...");
        syncTime(); // Synchroniseer de tijd met NTP als de RTC-tijd ongeldig is
    }
    else
    {
        Serial.println("✅ RTC heeft een geldige tijd.");
        timeSynced = true;
    }
}

void PrioDateTime::syncTime() {
    Serial.println("Synchroniseren met NTP-server...");

    // Stel de tijdzone in op UTC (zonder zomer/wintertijdcorrectie)
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

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
            // Pas de tijdzone aan op basis van zomer/wintertijd
            setTimeZone(&timeinfo);

            RtcDateTime compiledDateTime(
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec
            );
            _rtc.SetDateTime(compiledDateTime);
            Serial.println("RTC bijgewerkt met NTP-tijd.");
        }
    }

    _lastSyncTime = millis(); // Update de laatste synchronisatietijd
}

void PrioDateTime::checkSync()
{
    unsigned long currentTime = millis();

    // Controleer of het synchronisatie-interval is verstreken
    if (currentTime - _lastSyncTime >= _syncInterval)
    {
        syncTime(); // Voer synchronisatie uit
    }
}
bool PrioDateTime::isTimeSynced()
{
    return timeSynced;
}

char *PrioDateTime::getTime()
{
    RtcDateTime now = _rtc.GetDateTime(); // Lees de tijd van de RTC

    snprintf(buffer, sizeof(buffer), "%02d:%02d", now.Hour(), now.Minute());
    return buffer;
}

char *PrioDateTime::getDate()
{
    RtcDateTime now = _rtc.GetDateTime(); // Lees de datum van de RTC

    snprintf(buffer, sizeof(buffer), "%02d-%02d-%04d", now.Day(), now.Month(), now.Year());
    return buffer;
}

char *PrioDateTime::getDateTime()
{
    RtcDateTime now = _rtc.GetDateTime(); // Lees datum en tijd van de RTC

    snprintf(buffer, sizeof(buffer), "%02d-%02d-%04d %02d:%02d",
             now.Day(), now.Month(), now.Year(), now.Hour(), now.Minute());
    return buffer;
}

bool PrioDateTime::isSummerTime(tm *timeinfo)
{
    // Zomertijd begint op de laatste zondag van maart om 02:00
    // Wintertijd begint op de laatste zondag van oktober om 03:00

    if (timeinfo->tm_mon < 2 || timeinfo->tm_mon > 9)
    {
        // Jan, Feb, Nov, Dec: altijd wintertijd
        return false;
    }
    else if (timeinfo->tm_mon > 2 && timeinfo->tm_mon < 9)
    {
        // Apr, May, Jun, Jul, Aug, Sep: altijd zomertijd
        return true;
    }
    else
    {
        // Maart en oktober: controleer de laatste zondag
        int lastSunday = (31 - (5 * timeinfo->tm_year / 4 + 4) % 7); // Laatste zondag van de maand

        if (timeinfo->tm_mon == 2)
        { // Maart
            return (timeinfo->tm_mday > lastSunday ||
                    (timeinfo->tm_mday == lastSunday && timeinfo->tm_hour >= 2));
        }
        else
        { // Oktober
            return (timeinfo->tm_mday < lastSunday ||
                    (timeinfo->tm_mday == lastSunday && timeinfo->tm_hour < 3));
        }
    }
}
void PrioDateTime::setTimeZone(tm *timeinfo) {
    if (isSummerTime(timeinfo)) {
        // Zomertijd: UTC+2
        setenv("TZ", "CEST", 1);
    } else {
        // Wintertijd: UTC+1
        setenv("TZ", "CET-1", 1);
    }
    tzset(); // Pas de tijdzone-instelling toe
}