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

    QueueHandle_t AudioQueue = static_cast<QueueHandle_t>(parameter);

    audio.setPinout(DAC_I2S_BCLK, DAC_I2S_LRC, DAC_I2S_DOUT);
    audio.setVolume(DEF_VOLUME);
    audio.setVolumeSteps(VOLUME_STEPS);

    // Wacht totdat zowel de DisplayTask als de webServerTask zijn gestart
    xEventGroupWaitBits(
        taskEvents,                                            // Event group
        DISPLAY_TASK_STARTED_BIT | WEBSERVER_TASK_STARTED_BIT, // Bits om op te wachten
        pdTRUE,                                                // Clear de bits na het wachten
        pdTRUE,                                                // Wacht op ALLE bits
        portMAX_DELAY);                                        // Wacht oneindig lang

    while (true)
    {
        AudioData audioData;
        if (xQueueReceive(AudioQueue, &audioData, 0) == pdTRUE)
        {

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
                break;

            case CMD_STOP:
                Serial.println("Audio STOP commando ontvangen");
                audio.stopSong();
                current_url = "";
                paused = false;
                break;

            case CMD_PAUSE:
                Serial.println("Audio PAUSE commando ontvangen");
                paused = true;
                break;

            case CMD_RESUME:
                Serial.println("Audio RESUME commando ontvangen");
                paused = false;
                break;

            default:
                break;
            }

            // Serial.println("Data redceived for audio task");
            // if (current_volume != audioData.volume)
            // {
            //     Serial.println("Volume changed to: " + String(audioData.volume));
            //     audio.setVolume(audioData.volume);
            //     current_volume = audioData.volume;
            // }

            // const String streamUrl = String(audioData.url);
            // if (streamUrl != current_url)
            // {
            //     Serial.println("Switching to stream: " + streamUrl);
            //     Serial.println("playing:" + streamUrl);
            //     audio.connecttohost(audioData.url);
            //     current_url = streamUrl;
            //     //                myPrefs.writeString("lasturl", _audioData.url);
            //     //                myPrefs.putUInt("stream_index", streamIndex);
            // }
        }
        //       Serial.println("playing audio");
        audio.loop();
        vTaskDelay(10);
    }
}
