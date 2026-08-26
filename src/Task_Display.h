#ifndef TaskDisplay_H
#define TaskDisplay_H
#include <Arduino.h>
#include "globals.h"
#include "Adafruit_VEML7700.h"
#include "driver/ledc.h" // Include LEDC driver header for PWM functionality
#include "PrioTft.h"
#include "PrioRotary.h"
#include "PrioRotaryMenu.h"
#include "MyPreferences.h"
#include "AudioControl.h"
#include <ezButton.h> 
#include "globals.h"





void onMenuAction(const char* action);
void onMenuOpen();
void onMenuClose();

void checkVolume();
void DisplayTask(void *parameter);
// PrioDlnaBrowser::PlayCallback target: starts playback of a DLNA track and
// pushes its DIDL-Lite metadata (title/artist/album/albumArtURI) to the
// display queue, reusing the same title/logo fields presets use. Defined in
// main.cpp since that's where the persistent DisplayData/SendDataToDisplay()
// state lives. Fields may be "?" (DLNA-not-present sentinel) or empty.
void playDlnaTrack(const char* url, const char* title, const char* artist,
                    const char* album, const char* albumArtURI);
// PrioDlnaBrowser::CancelledCallback target: resumes whatever preset was
// playing before the DLNA browser stopped it (see PrioDlnaBrowser::start()),
// falling back to the first favorite if none is known yet. Defined in
// main.cpp, where stream_index/UrlManagerInstance/playStream() live.
void resumePreviousStream();
void spl(String mes);
void cleanStreamTitle(struct DisplayData * data);
void AdjustBackLight(Adafruit_VEML7700 veml);
void SetBacklightPWM(int brightness);
int mapLuxToPWM(float lux);
void setup_backlight() ;
struct DisplayData {
    char title[100];
    char logo[255];
    char ip[16];
    char bitrate[10];
    char station[100];
    char icyurl[255];
    char lasthost[255];
    char streamtitle[255];
    char alarmState[32];
    int volume;
    bool loadingState;
    bool standbyState;
    char currenTime[6]; // "HH:MM\0" - was [5], one byte short of the null terminator
    char currenDate[20];

};




#endif // TaskDisplay_H