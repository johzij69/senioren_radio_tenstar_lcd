#include "Task_Audio.h"
#include "Audio.h"
#include "Task_Shared.h"
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
                Serial.printf("[AUDIO] CMD_PLAY received - URL: '%s', Volume: %d\n", audioData.url, audioData.volume);
                Serial.printf("[AUDIO] Current URL: '%s', Current Volume: %d\n", current_url.c_str(), current_volume);
                
                if (audioData.volume != current_volume)
                {
                    audio.setVolume(audioData.volume);
                    current_volume = audioData.volume;
                    Serial.println("Volume aangepast naar: " + String(current_volume));
                }

                if (audioData.url[0] != '\0')
                {
                    // ALWAYS reconnect on CMD_PLAY to ensure stream starts properly
                    // This fixes stuck streams and ensures fresh connection
                    Serial.println("[AUDIO] Starting/restarting stream: " + String(audioData.url));
                    Serial.printf("[AUDIO] Free heap before: %d bytes\n", ESP.getFreeHeap());
                    
                    // Stop current stream if playing
                    if (current_url.length() > 0) {
                        Serial.println("[AUDIO] Stopping current stream first");
                        audio.stopSong();
                        vTaskDelay(500 / portTICK_PERIOD_MS); // Give it time to stop and free buffers
                    }
                    
                    Serial.printf("[AUDIO] Calling connecttohost: %s\n", audioData.url);
                    bool connected = audio.connecttohost(audioData.url);
                    Serial.printf("[AUDIO] connecttohost returned: %d\n", connected);
                    Serial.printf("[AUDIO] Free heap after: %d bytes\n", ESP.getFreeHeap());
                    
                    current_url = String(audioData.url);
                    paused = false;
                    
                    // Give audio.loop() time to start connection
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    Serial.printf("[AUDIO] audio.isRunning(): %d\n", audio.isRunning());
                }
                else
                {
                    Serial.println("[AUDIO] ERROR: Empty URL received!");
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

            case CMD_SET_VOLUME:
                if (audioData.volume != current_volume)
                {
                    audio.setVolume(audioData.volume);
                    current_volume = audioData.volume;
                    Serial.println("Volume aangepast naar: " + String(current_volume));
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
        vTaskDelay(5 / portTICK_PERIOD_MS); // Shorter delay for faster audio.loop() calls
    }
}
