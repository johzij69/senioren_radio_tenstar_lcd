#include "AudioControl.h"
#include "task_shared.h"
#include "globals.h"
#include "Task_Audio.h"  // Voor CMD_PLAY etc.
#include "freertos/queue.h"
#include "freertos/event_groups.h"

extern QueueHandle_t AudioQueue;
extern EventGroupHandle_t taskEvents;

void playAudio(const char *url, int volume)
{
    AudioData data;
    data.command = CMD_PLAY;
    data.volume = volume;
    strncpy(data.url, url, sizeof(data.url));
    xQueueSend(AudioQueue, &data, portMAX_DELAY);

    // Eventueel wachten op bevestiging (optioneel)
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
    data.command = CMD_PLAY; // Zelfde als play, maar met lege URL = alleen volume wijziging
    data.volume = volume;
    data.url[0] = '\0';  // Lege string betekent: zelfde stream
    xQueueSend(AudioQueue, &data, portMAX_DELAY);
}
