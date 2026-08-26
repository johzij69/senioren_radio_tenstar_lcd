#include "Arduino.h"
#include "LittleFS.h"
// Deliberately no HTTPClient/WiFiClientSecure here - see fetchImage.cpp for why.
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <WiFiClient.h>


// Downloads url (http:// only) into filename on LittleFS if not already cached there.
bool getFile(String url, String filename);
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);