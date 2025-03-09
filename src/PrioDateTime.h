#ifndef PRIO_DATETIME_H
#define PRIO_DATETIME_H

#include <Arduino.h>
#include <ctime>
#include <ThreeWire.h>
#include <RtcDS1302.h>

class PrioDateTime {
public:
    PrioDateTime(int clkPin = 14, int datPin = 27, int rstPin = 26); // Constructor met standaardpinnen
    void begin();
    char* getTime();        // HH:MM teruggeven
    char* getDate();        // DD-MM-YYYY teruggeven
    char* getDateTime();    // DD-MM-YYYY HH:MM teruggeven

    bool timeSynced;        // Publieke variabele om status te controleren

private:
    void syncTime();        // Tijd synchroniseren met timeout
    char buffer[20];        // Buffer voor datum/tijd strings
    ThreeWire _threeWire;   // ThreeWire instantie
    RtcDS1302<ThreeWire> _rtc; // RTC instantie
    int _clkPin;            // CLK-pin
    int _datPin;            // DAT-pin
    int _rstPin;            // RST-pin
};

#endif