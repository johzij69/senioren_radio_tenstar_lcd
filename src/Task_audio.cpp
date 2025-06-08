#include "Task_Audio.h"
#include "Audio.h"
#include "task_shared.h"
#include "globals.h"

void AudioTask(void *parameter)
{
    Audio audio;
    String current_url = "";
    int current_volume = MIN_VOLUME;
    bool paused = false;
    bool wasRunning = false;

    QueueHandle_t AudioQueue = static_cast<QueueHandle_t>(parameter);

    audio.setPinout(DAC_I2S_BCLK, DAC_I2S_LRC, DAC_I2S_DOUT);
    audio.setVolume(DEF_VOLUME);
    audio.setVolumeSteps(VOLUME_STEPS);
    Serial.println("AudioTask: Audio object is initialized");
    xEventGroupWaitBits(
        taskEvents,
        DISPLAY_TASK_STARTED_BIT | WEBSERVER_TASK_STARTED_BIT,
        pdTRUE,
        pdTRUE,
        portMAX_DELAY);

    while (true)
    {
        AudioData audioData;
        if (xQueueReceive(AudioQueue, &audioData, 0) == pdTRUE)
        {
            
            Serial.println("Audio commando ontvangen: " + String(audioData.command));
            switch (audioData.command)
            {
            case CMD_PLAY:
                if (audioData.volume != current_volume)
                {
                    audio.setVolume(audioData.volume);
                    current_volume = audioData.volume;
                    Serial.println("Volume aangepast naar: " + String(current_volume));
                }

                if (String(audioData.url) != current_url)
                {
                    Serial.println("Switching to stream: " + String(audioData.url));
                    audio.connecttohost(audioData.url);
                    current_url = String(audioData.url);
                    paused = false;
                }


                xEventGroupSetBits(taskEvents, AUDIO_STARTED_BIT);
                break;

            case CMD_STOP:
                Serial.println("Audio STOP commando ontvangen");
                audio.stopSong();
                current_url = "";
                paused = false;

                break;

            case CMD_PAUSE:
                Serial.println("Audio PAUSE commando ontvangen");
                if (!paused)
                {
                    audio.pauseResume();
                    paused = true;
                    Serial.println("Audio is gepauzeerd");
                    xEventGroupSetBits(taskEvents, AUDIO_PAUSED_BIT);
                }
                break;

            case CMD_RESUME:
                Serial.println("Audio RESUME commando ontvangen");
                if (paused)
                {
                    audio.pauseResume();
                    paused = false;
                    Serial.println("Audio hervat");
                    xEventGroupSetBits(taskEvents, AUDIO_STARTED_BIT);
                }
                break;

            default:
                break;
            }
        }

        audio.loop();

        bool isNowRunning = audio.isRunning();

        if (wasRunning && !isNowRunning && !paused)
        {
            Serial.println("Audio naar standby");
            xEventGroupSetBits(taskEvents, AUDIO_STOPPED_BIT);
        }

        wasRunning = isNowRunning;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
