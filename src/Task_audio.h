#ifndef TaskAudio_H
#define TaskAudio_H
#include <Arduino.h>

void AudioTask(void *parameter);



struct AudioData {
    char url[255];
    int volume;
};


#endif // TaskAudio_H







