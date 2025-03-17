#ifndef GeneralHelpers_H
#define GeneralHelpers_H
#include <Arduino.h>

void printTaskCore(TaskHandle_t taskHandle, const char *taskName);
void printBinary(int v, int num_places);  
void searchAndReplace(String *htmlString, String findPattern, String replaceWith);

#endif