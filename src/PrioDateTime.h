#ifndef PRIO_DATETIME_H
#define PRIO_DATETIME_H

#include <Arduino.h>
#include <ctime>

class PrioDateTime {
public:
    PrioDateTime();
    void begin();
    char* getTime();        // HH:MM teruggeven
    char* getDate();        // DD-MM-YYYY teruggeven
    char* getDateTime();    // DD-MM-YYYY HH:MM teruggeven

    bool timeSynced;        // Publieke variabele om status te controleren

private:
    void syncTime();        // Tijd synchroniseren met timeout
    char buffer[20];        // Buffer voor datum/tijd strings
};

#endif
