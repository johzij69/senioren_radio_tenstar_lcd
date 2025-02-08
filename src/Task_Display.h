#ifndef TaskDisplay_H
#define TaskDisplay_H
#include <Arduino.h>

void DisplayTask(void *parameter);
void spl(String mes);

struct DisplayData {
    char title[100];
    char logo[255];
    char ip[16];
    int volume;
};

#endif // TaskDisplay_H