#ifndef TaskDisplay_H
#define TaskDisplay_H
#include <Arduino.h>



void DisplayTask(void *parameter);
void spl(String mes);
void cleanStreamTitle(struct DisplayData * data);
struct DisplayData {
    char title[100];
    char logo[255];
    char ip[16];
    char bitrate[10];
    char station[100];
    char icyurl[255];
    char lasthost[255];
    char streamtitle[255];
    int volume;

    char currenTime[5];

};





#endif // TaskDisplay_H