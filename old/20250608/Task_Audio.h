#ifndef TaskAudio_H
#define TaskAudio_H
#include <Arduino.h>


// Commandotypes
enum AudioCommandType {
    CMD_NONE,
    CMD_PLAY,
    CMD_STOP,
    CMD_PAUSE,
    CMD_RESUME
};

struct AudioData {
    char url[255];
    int volume = 10; // Default volume
    AudioCommandType command = CMD_NONE;
};


void AudioTask(void *parameter);

#endif // TaskAudio_H







