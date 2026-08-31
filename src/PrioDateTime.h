#ifndef PRIODATETIME_H
#define PRIODATETIME_H

#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <WiFi.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class PrioDateTime {
public:
    // Constructor
    PrioDateTime(int clkPin, int datPin, int rstPin);

    // Initialiseer de RTC en synchroniseer indien nodig
    void begin();

    // Synchroniseer de tijd met de NTP-server; true als de sync is gelukt
    bool syncTime();

    // 12- of 24-uurs weergave voor getTime()
    void set24HourMode(bool use24Hour);
    bool is24HourMode() const;

    // Geeft aan of de tijd succesvol is gesynchroniseerd
    bool isTimeSynced();

    // Controleer of synchronisatie nodig is en voer deze uit
    void checkSync();

    // Haal de huidige tijd op (formaat: "HH:MM")
    char* getTime();

    // Haal de huidige datum op (formaat: "DD-MM-JJJJ")
    char* getDate();

    // Haal de huidige dag+datum op (formaat: "Wo DD-MM-JJJJ")
    char* getDayDate();

    // Haal de huidige datum en tijd op (formaat: "DD-MM-JJJJ HH:MM")
    char* getDateTime();

    // Controleer of de zomertijd actief is
    bool isSummerTime(tm* timeinfo);

    void setTimeZone(tm *timeinfo);

    bool debug ; // Debugmodus (standaard uitgeschakeld)

private:
    ThreeWire _threeWire; // ThreeWire-object voor communicatie met de RTC
    RtcDS1302<ThreeWire> _rtc; // RTC-object
    // _rtc is bit-banged over shared GPIO pins and buffer is written by every
    // get*() call below - both are touched from DisplayTask, the main loop
    // task (updateClockDisplay(), CreateAndSendDisplayData(), playDlnaTrack())
    // and the webserver task (/api/synctime), so every access needs to go
    // through this mutex or two tasks racing on the same read/write can tear
    // the buffer content or the RTC's bit-bang timing.
    SemaphoreHandle_t _mutex;

    bool timeSynced; // Geeft aan of de tijd succesvol is gesynchroniseerd
    bool _use24Hour = true;
    unsigned long _lastSyncTime; // Laatste synchronisatietijd (in milliseconden)
    unsigned long _syncInterval; // Interval tussen synchronisaties (in milliseconden)

    char buffer[20]; // Buffer voor het opslaan van tijd- en datumstrings
};

#endif