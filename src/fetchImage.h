#include "Arduino.h"
#include "LittleFS.h"
#include <HTTPClient.h>
#include <TFT_eSPI.h>  

bool getFile(String url, String filename);
// bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);