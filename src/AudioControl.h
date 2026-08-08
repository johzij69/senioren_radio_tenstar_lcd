#pragma once

#include "Arduino.h"
#include "globals.h"

// Deze functies kunnen vanuit main.cpp of andere onderdelen worden aangeroepen
void playAudio(const char *url );
void pauseAudio();
void resumeAudio();
void stopAudio();




// Optioneel: helper om volume te wijzigen zonder te herstarten
void setAudioVolume(int volume);
