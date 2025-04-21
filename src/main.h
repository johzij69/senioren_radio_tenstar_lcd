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
#include <freertos/event_groups.h>
#include "UrlManager.h"
#include "PrioWebserver.h"
#include "task_webServer.h"
#include "globals.h" 


#define WEB_SERVER_PORT 80

// Rotary Enocder used for Volume control
#define ROT_CLK_PIN 17  // ESP32 pin GPIO17 connected to the rotary encoder's CLK pin
#define ROT_DT_PIN 16   // ESP32 pin GPIO16 connected to the rotary encoder's DT pin
#define ROT_SW_PIN 15   // ESP32 pin GPIO15 connected to the rotary encoder's SW pin

#define DIRECTION_CW 0  // clockwise direction
#define DIRECTION_CCW 1 // counter-clockwise direction

// /* Next Button */
// #define NEXT_BUTTON_PIN 21 // ESP32 pin GPIO21, which connected to button
#define VOLUME_STEPS 30
#define MIN_VOLUME 0
#define MAX_VOLUME 30
#define DEF_VOLUME 15

// Input panel
#define INPUTPANEL_ADDRESS 0x20
#define INPUTPANEL_INT_PIN 45 //21
#define INPUTPANEL_SDA 47
#define INPUTPANEL_SCL 48

// RF Receiver
#define RF_RECEIVER_PIN 1

// Back light
#define BACKLIGHT_PIN 18 // GPIO pin for backlight control

// RTC
#define RTC_CLK_PIN 21
#define RTC_DAT_PIN 14
#define RTC_RST_PIN 13



void checkVolume();
void CreateAndSendDisplayData(int streamIndex);
void CreateAndSendAudioData(int streamIndex, int last_volume);
void printBinary(int v, int num_places);
void usePreset(int presetNumber);
void setup_backlight() ;
void sync_time();


