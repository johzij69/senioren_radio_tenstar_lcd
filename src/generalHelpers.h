#ifndef GeneralHelpers_H
#define GeneralHelpers_H
#include <Arduino.h>

void printTaskCore(TaskHandle_t taskHandle, const char *taskName);
void printBinary(int v, int num_places);  
void searchAndReplace(String *htmlString, String findPattern, String replaceWith);
void enableTlsPsramAllocator();

// Pagina-gewijs scrollen: het venster verspringt een hele pagina zodra de selectie
// erbuiten valt (vooruit komt de selectie bovenaan, terug onderaan te staan).
// Geeft true als de pagina wisselde en de lijst dus opnieuw getekend moet worden.
bool updatePageOffset(int selectedIndex, int totalItems, int visibleRows, int &scrollOffset);

#endif