#ifndef PRIODATETIME_H
#define PRIODATETIME_H

#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <WiFi.h>
#include <time.h>

class PrioDateTime {
public:
    // Constructor
    PrioDateTime(int clkPin, int datPin, int rstPin);

    // Initialiseer de RTC en synchroniseer indien nodig
    void begin();

    // Synchroniseer de tijd met de NTP-server
    void syncTime();

    // Geeft aan of de tijd succesvol is gesynchroniseerd
    bool isTimeSynced();

    // Controleer of synchronisatie nodig is en voer deze uit
    void checkSync();

    // Haal de huidige tijd op (formaat: "HH:MM")
    char* getTime();

    // Haal de huidige datum op (formaat: "DD-MM-JJJJ")
    char* getDate();

    // Haal de huidige datum en tijd op (formaat: "DD-MM-JJJJ HH:MM")
    char* getDateTime();

    // Controleer of de zomertijd actief is
    bool isSummerTime(tm* timeinfo);

    void setTimeZone(tm *timeinfo);

private:
    ThreeWire _threeWire; // ThreeWire-object voor communicatie met de RTC
    RtcDS1302<ThreeWire> _rtc; // RTC-object

    bool timeSynced; // Geeft aan of de tijd succesvol is gesynchroniseerd
    unsigned long _lastSyncTime; // Laatste synchronisatietijd (in milliseconden)
    unsigned long _syncInterval; // Interval tussen synchronisaties (in milliseconden)

    char buffer[20]; // Buffer voor het opslaan van tijd- en datumstrings
};

#endif