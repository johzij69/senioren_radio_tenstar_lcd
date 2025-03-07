#include "Arduino.h"
#include <SPI.h>
#include <TFT_eSPI.h>

#define WM_ERASE_NVS

// Rotary Enocder used for Volume control
#define ROT_CLK_PIN 17  // ESP32 pin GPIO17 connected to the rotary encoder's CLK pin
#define ROT_DT_PIN 16   // ESP32 pin GPIO16 connected to the rotary encoder's DT pin
#define ROT_SW_PIN 15   // ESP32 pin GPIO15 connected to the rotary encoder's SW pin
#define DIRECTION_CW 0  // clockwise direction
#define DIRECTION_CCW 1 // counter-clockwise direction

/* Next Button */
#define NEXT_BUTTON_PIN 21 // ESP32 pin GPIO21, which connected to button


#define VOLUME_STEPS 30
#define MIN_VOLUME 0
#define DEF_VOLUME 15

#define PRESET1_BUTTON_PIN 1
#define PRESET2_BUTTON_PIN 2
#define PRESET3_BUTTON_PIN 42
#define PRESET4_BUTTON_PIN 41
#define PRESET5_BUTTON_PIN 40
#define PRESET6_BUTTON_PIN 39
#define PRESET7_BUTTON_PIN 38
#define PRESET8_BUTTON_PIN 45
#define PRESET9_BUTTON_PIN 48
#define PRESET10_BUTTON_PIN 47



void CreateAndSendDisplayData(int streamIndex);
void CreateAndSendAudioData(int streamIndex, int last_volume);


// Definieer de GPIO pinnen voor de buttons
const int buttonPins[10] =  {PRESET1_BUTTON_PIN,
                             PRESET2_BUTTON_PIN,
                             PRESET3_BUTTON_PIN,
                             PRESET4_BUTTON_PIN,
                             PRESET5_BUTTON_PIN,
                             PRESET6_BUTTON_PIN,
                             PRESET7_BUTTON_PIN,
                             PRESET8_BUTTON_PIN,
                             PRESET9_BUTTON_PIN,
                             PRESET10_BUTTON_PIN};

// Declareer functies
void usePreset(int presetNumber);


