#pragma once
#ifndef TASK_AUDIO_H
#define TASK_AUDIO_H
#include <Arduino.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/queue.h>
// #include "freertos/task.h"
// #include "audioPlayerDefaults.h"
#include "Audio.h"

struct AudioQueues {
    QueueHandle_t VolumeQ;
    QueueHandle_t StreamQ;
};


void AudioTask(void *parameter);



// struct TaskParameters {
//     QueueHandle_t queue1;
//     QueueHandle_t queue2;
// };


#endif // TASK_AUDIO_H