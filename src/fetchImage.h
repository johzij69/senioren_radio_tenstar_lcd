#include "Arduino.h"
#include "LittleFS.h"
#include <HTTPClient.h>
// WiFiClientSecure removed - conflicts with Audio library SSL
#include <WiFi.h>
#include <TFT_eSPI.h>  
#include <WiFiClient.h>


bool getFileOld(String url, String filename);
bool getFile(String url, String filename);  // Now disabled - returns false
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);