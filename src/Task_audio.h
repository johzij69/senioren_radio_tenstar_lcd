#ifndef TaskAudio_H
#define TaskAudio_H
#include <Arduino.h>


// Commandotypes
enum AudioCommandType {
    CMD_NONE,
    CMD_PLAY,
    CMD_STOP,
    CMD_PAUSE,
    CMD_RESUME,
    CMD_SET_VOLUME
};

struct AudioData {
    char url[255];
    int volume = 10; // Default volume
    AudioCommandType command = CMD_NONE;
    bool isDlnaTrack = false; // set by playAudio(url, true) - see audio_eof_stream in Task_audio.cpp
};

// Outbound events from AudioTask, mirroring the DlnaEvent/DlnaEventQueue pattern.
enum AudioEventType {
    AUDIO_EVT_TRACK_ENDED // a DLNA track reached its natural end (audio_eof_stream)
};

struct AudioEvent {
    AudioEventType type;
};

void AudioTask(void *parameter);

#endif // TaskAudio_H







