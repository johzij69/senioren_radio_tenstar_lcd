#ifndef TaskAudio_H
#define TaskAudio_H
#include <Arduino.h>

void _AudioTask(void *parameter);


struct AudioQueues {
    QueueHandle_t VolumeQ;
    QueueHandle_t StreamQ;
};

extern AudioQueues audioQueue;

#endif // TaskAudio_H







