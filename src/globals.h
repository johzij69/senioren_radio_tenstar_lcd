// globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

#include "PrioDateTime.h"  // Zorg dat de header van PrioDateTime bekend is
#include "PrioInputPanel.h"
#include "PrioRfReceiver.h"


/*
* GPIO PINS
*/
// RTC
#define RTC_DAT_PIN 1 
#define RTC_CLK_PIN 2 
#define RTC_RST_PIN 3 

// RF Receiver
#define RF_RECEIVER_PIN 4 

// DAC I2S
#define DAC_I2S_LRC 5  // LCK
#define DAC_I2S_DOUT 6 // DIN
#define DAC_I2S_BCLK 7 // BCK



// Rotary Enocder 
#define ROT_SW_PIN 15   
#define ROT_DT_PIN 16   
#define ROT_CLK_PIN 17  
#define DIRECTION_CW 0  // clockwise direction
#define DIRECTION_CCW 1 // counter-clockwise direction

#define VOLUME_STEPS 30
#define MIN_VOLUME 0
#define DEF_VOLUME 10
#define MAX_VOLUME 30

// Back light
#define BACKLIGHT_PIN 18 

// Power Button
#define POWER_BUTTON_PIN 46

// Input panel
#define TOPPANEL_INT_PIN 45 
#define TOPPANEL_SDA 47
#define TOPPANEL_SCL 48
#define TOPPANEL_ADDRESS 0x20

#define WEB_SERVER_PORT 80
















extern PrioDateTime pDateTime;  // Declareer als extern (definitie komt elders)
extern PrioInputPanel inputPanel;
extern PrioRfReceiver rfReceiver;

#endif