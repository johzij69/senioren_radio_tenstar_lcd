#pragma once

#include "Arduino.h"

// Deze functies kunnen vanuit main.cpp of andere onderdelen worden aangeroepen
void playAudio(const char *url, int volume);
void pauseAudio();
void resumeAudio();
void stopAudio();

// Optioneel: helper om volume te wijzigen zonder te herstarten
void setAudioVolume(int volume);
