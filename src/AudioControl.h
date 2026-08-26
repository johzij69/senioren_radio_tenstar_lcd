#pragma once

#include "Arduino.h"
#include "globals.h"

// Deze functies kunnen vanuit main.cpp of andere onderdelen worden aangeroepen
// isDlnaTrack: marks the stream as a DLNA folder track, so its natural end
// (audio_eof_stream in Task_audio.cpp) triggers auto-advance to the next track
// in the folder instead of being ignored (see PrioDlnaBrowser::playNext()).
void playAudio(const char *url, bool isDlnaTrack = false);
void pauseAudio();
void resumeAudio();
void stopAudio();




// Optioneel: helper om volume te wijzigen zonder te herstarten
void setAudioVolume(int volume);
