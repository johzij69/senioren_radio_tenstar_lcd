#include "PrioDateTime.h"

PrioDateTime::PrioDateTime(int clkPin, int datPin, int rstPin)
    : _threeWire(datPin, clkPin, rstPin), _rtc(_threeWire)
{                                    // Initialiseer ThreeWire en RtcDS1302
    timeSynced = false;              // Standaard niet gesynchroniseerd
    _lastSyncTime = 0;               // Laatste synchronisatietijd (in milliseconden)
    _syncInterval = 1 * 3600 * 1000; // Synchroniseer elke 1 uur (1 uur * 3600 seconden * 1000 ms)
    _mutex = xSemaphoreCreateMutex();
}

void PrioDateTime::begin()
{
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _rtc.Begin();
    if (debug)
    {
        Serial.println("RTC initialiseren...");
        delay(5000); // Wacht 1 seconde voor de initialisatie
    }
    bool wasValid = _rtc.IsDateTimeValid();
    xSemaphoreGive(_mutex);

    // "Valid" alleen betekent dat de RTC-chip niet is gecrasht/leeg is - niet dat
    // de opgeslagen tijd ook klopt. Zonder deze onvoorwaardelijke sync bleef een
    // verlopen/verkeerd gezette RTC-tijd (bv. na een gemiste zomertijdwissel of
    // gewoon kristaldrift) onopgemerkt staan totdat iemand handmatig /api/synctime
    // aanriep - checkSync() in loop() ving dat pas na een vol uur op.
    if (!wasValid)
    {
        Serial.println("⛔ RTC heeft geen geldige tijd! Synchroniseren met NTP...");
    }
    else
    {
        Serial.println("✅ RTC heeft een geldige tijd, synchroniseer alsnog met NTP om drift/zomertijd te corrigeren...");
    }
    syncTime(); // heeft zijn eigen lock rond het _rtc.SetDateTime() gedeelte, zie hieronder
}

void PrioDateTime::syncTime()
{
    Serial.println("Synchroniseren met NTP-server...");

    // Gebruik een volledige TZ-regel voor Nederland (CET/CEST met DST)
    configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org", "time.nist.gov");

    // Wachten op tijdsynchronisatie (max 20 sec)
    int timeout = 20;
    time_t now = time(nullptr);
    while (now < 1000000000 && timeout > 0)
    {
        delay(1000);
        now = time(nullptr);
        Serial.print(".");
        timeout--;
    }

    if (timeout == 0)
    {
        Serial.println("\n⛔ Tijd synchronisatie mislukt!");
        timeSynced = false;
    }
    else
    {
        Serial.println("\n✅ Tijd gesynchroniseerd!");
        timeSynced = true;

        // Werk de RTC bij met de gesynchroniseerde tijd

        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            RtcDateTime compiledDateTime(
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            xSemaphoreTake(_mutex, portMAX_DELAY);
            _rtc.SetDateTime(compiledDateTime);
            xSemaphoreGive(_mutex);
            Serial.println("RTC bijgewerkt met NTP-tijd.");
            Serial.println("Tijd: " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec));
            Serial.println("Datum: " + String(timeinfo.tm_mday) + "-" + String(timeinfo.tm_mon + 1) + "-" + String(timeinfo.tm_year + 1900));
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
        Serial.println("⏰ Synchronisatie-interval verstreken. Tijd synchroniseren...");

        syncTime(); // Voer synchronisatie uit
    }
}
bool PrioDateTime::isTimeSynced()
{
    return timeSynced;
}

// NB: still returns a pointer into the shared `buffer` member, so callers
// must copy the result out (e.g. via strncpy) before any other task gets a
// chance to call another get*() and overwrite it - the lock only guarantees
// the RTC read + buffer write itself isn't torn by a concurrent call.
char *PrioDateTime::getTime()
{
    xSemaphoreTake(_mutex, portMAX_DELAY);
    RtcDateTime now = _rtc.GetDateTime(); // Lees de tijd van de RTC
    snprintf(buffer, sizeof(buffer), "%02d:%02d", now.Hour(), now.Minute());
    xSemaphoreGive(_mutex);
    return buffer;
}

char *PrioDateTime::getDate()
{
    xSemaphoreTake(_mutex, portMAX_DELAY);
    RtcDateTime now = _rtc.GetDateTime(); // Lees de datum van de RTC
    snprintf(buffer, sizeof(buffer), "%02d-%02d-%04d", now.Day(), now.Month(), now.Year());
    xSemaphoreGive(_mutex);
    return buffer;
}

char *PrioDateTime::getDayDate()
{
    xSemaphoreTake(_mutex, portMAX_DELAY);
    RtcDateTime now = _rtc.GetDateTime();

    tm timeinfo = {};
    timeinfo.tm_year = now.Year() - 1900;
    timeinfo.tm_mon = now.Month() - 1;
    timeinfo.tm_mday = now.Day();
    timeinfo.tm_hour = now.Hour();
    timeinfo.tm_min = now.Minute();
    timeinfo.tm_sec = now.Second();
    timeinfo.tm_isdst = -1;
    mktime(&timeinfo);

    static const char* dayNames[] = {"Zo", "Ma", "Di", "Wo", "Do", "Vr", "Za"};
    uint8_t dayIndex = (timeinfo.tm_wday >= 0 && timeinfo.tm_wday <= 6) ? (uint8_t)timeinfo.tm_wday : 0;

    snprintf(buffer, sizeof(buffer), "%s %02d-%02d-%04d",
             dayNames[dayIndex], now.Day(), now.Month(), now.Year());
    xSemaphoreGive(_mutex);
    return buffer;
}

char *PrioDateTime::getDateTime()
{
    xSemaphoreTake(_mutex, portMAX_DELAY);
    RtcDateTime now = _rtc.GetDateTime(); // Lees datum en tijd van de RTC
    snprintf(buffer, sizeof(buffer), "%02d-%02d-%04d %02d:%02d",
             now.Day(), now.Month(), now.Year(), now.Hour(), now.Minute());
    xSemaphoreGive(_mutex);
    return buffer;
}

bool PrioDateTime::isSummerTime(tm *timeinfo)
{
    // Zomertijd begint op de laatste zondag van maart om 02:00
    // Wintertijd begint op de laatste zondag van oktober om 03:00

    if (timeinfo->tm_mon < 2 || timeinfo->tm_mon > 9)
    {
        // Jan, Feb, Nov, Dec: altijd wintertijd
        Serial.println("Wintertijd: " + String(timeinfo->tm_mon));
        return false;
    }
    else if (timeinfo->tm_mon > 2 && timeinfo->tm_mon < 9)
    {
        // Apr, May, Jun, Jul, Aug, Sep: altijd zomertijd
        Serial.println("Zomertijd: " + String(timeinfo->tm_mon));
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
void PrioDateTime::setTimeZone(tm *timeinfo)
{
    (void)timeinfo;
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();
}