#include "Arduino.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFiManager.h>
#include <ezButton.h> 
#include "PrioRotary.h"
#include "PrioDateTime.h"
#include "generalHelpers.h"
#include "MyPreferences.h"
#include "Task_Audio.h"
#include "Task_Display.h"
#include "Task_Shared.h"
#include "PrioTft.h"
#include "Free_Fonts.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <freertos/event_groups.h>
#include "UrlManager.h"
#include "PrioWebserver.h"
#include "task_webServer.h"
#include <Adafruit_NeoPixel.h>
#include "globals.h" 



bool inStandby = false; // Flag to indicate if the system is in standby mode

void checkVolume();
void handlePowerButtonInterrupt() ;
void CreateAndSendDisplayData(int streamIndex);
void CreateAndSendAudioData(int streamIndex, int last_volume);
void printBinary(int v, int num_places);
void usePreset(int presetNumber);
void setup_backlight() ;
void sync_time();
void startDisplayTask();
void startWebServerTask();
void startAudioTask();
void SendDataToDisplay();

