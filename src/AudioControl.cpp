#include "AudioControl.h"
#include "Task_Shared.h"
#include "globals.h"
#include "Task_Audio.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

extern QueueHandle_t AudioQueue;
extern EventGroupHandle_t taskEvents;

void playAudio(const char *url)
{
    AudioData data;
    data.command = CMD_PLAY;
    data.volume = last_volume;
    strncpy(data.url, url ? url : "", sizeof(data.url));
    data.url[sizeof(data.url) - 1] = '\0';
    xQueueSend(AudioQueue, &data, portMAX_DELAY);
    xEventGroupWaitBits(taskEvents, AUDIO_STARTED_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
}

void pauseAudio()
{
    AudioData data;
    data.command = CMD_PAUSE;
    xQueueSend(AudioQueue, &data, portMAX_DELAY);
    xEventGroupWaitBits(taskEvents, AUDIO_PAUSED_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
}

void resumeAudio()
{
    AudioData data;
    data.command = CMD_RESUME;
    xQueueSend(AudioQueue, &data, portMAX_DELAY);
    xEventGroupWaitBits(taskEvents, AUDIO_STARTED_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
}

void stopAudio()
{
    AudioData data;
    data.command = CMD_STOP;
    xQueueSend(AudioQueue, &data, portMAX_DELAY);
    xEventGroupWaitBits(taskEvents, AUDIO_STOPPED_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
}

void setAudioVolume(int volume)
{
    AudioData data;
    data.command = CMD_SET_VOLUME;
    data.volume = volume;
    data.url[0] = '\0';
    xQueueSend(AudioQueue, &data, portMAX_DELAY);
}