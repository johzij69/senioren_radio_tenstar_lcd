#include "Task_Audio.h"
#include "Audio.h"
#include "Task_Shared.h"
#include "globals.h"

extern QueueHandle_t AudioEventQueue;

// Set on every CMD_PLAY, read by audio_eof_stream() below. audio_eof_stream()
// fires for both live radio streams and DLNA file URLs (both go through
// connecttohost()/processWebStream() - see Audio.cpp), so this flag is what
// tells us whether "the stream ended" should trigger DLNA auto-advance or be
// ignored (e.g. a radio stream that dropped its connection).
static volatile bool s_currentIsDlna = false;

// Weak callback provided by the Audio library (Audio.h) - fires once when a
// stream reaches its natural end. stopSong()/reconnecting does NOT trigger
// this (m_f_eof is only set from the byte-counted end-of-data path in
// Audio.cpp), so it won't fire spuriously when the user or CMD_PLAY itself
// stops the current stream to start a new one.
void audio_eof_stream(const char* lastHost)
{
    (void)lastHost;
    if (s_currentIsDlna) {
        AudioEvent evt{ AUDIO_EVT_TRACK_ENDED };
        xQueueSend(AudioEventQueue, &evt, 0);
    }
}

void AudioTask(void *parameter)
{
    Audio *audio;
    char current_url[256] = "";
    int current_volume = MIN_VOLUME;
    bool paused = false;
    bool wasRunning = false;

    QueueHandle_t AudioQueue = static_cast<QueueHandle_t>(parameter);

    audio = new Audio();
    audio->setPinout(DAC_I2S_BCLK, DAC_I2S_LRC, DAC_I2S_DOUT);
    audio->setVolume(DEF_VOLUME);
    audio->setVolumeSteps(VOLUME_STEPS);
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
            Serial.printf("Audio commando ontvangen: %d\n", audioData.command);
            switch (audioData.command)
            {
            case CMD_PLAY:
                Serial.printf("[AUDIO] CMD_PLAY - URL: '%s', Volume: %d\n", audioData.url, audioData.volume);
                Serial.printf("[AUDIO] Current URL: '%s', Current Volume: %d\n", current_url, current_volume);
                s_currentIsDlna = audioData.isDlnaTrack;

                if (audioData.volume != current_volume)
                {
                    audio->setVolume(audioData.volume);
                    current_volume = audioData.volume;
                    Serial.printf("Volume aangepast naar: %d\n", current_volume);
                }

                if (audioData.url[0] != '\0')
                {
                    Serial.println("[AUDIO] Starting/restarting stream");
                    Serial.printf("[AUDIO] Heap: %d | Largest block: %d | PSRAM: %d\n",
                                  ESP.getFreeHeap(), ESP.getMaxAllocHeap(), ESP.getFreePsram());

                    if (current_url[0] != '\0')
                    {
                        Serial.println("[AUDIO] Stopping current stream first");
                        audio->stopSong();
                        // BELANGRIJK: wacht tot interne SSL buffers volledig vrij zijn
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                    }

                    Serial.printf("[AUDIO] Heap before connect: %d | Largest block: %d\n",
                                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());

                    // Audio library volgt redirects vanzelf! Geen eigen resolver nodig.
                    bool connected = audio->connecttohost(audioData.url);
                    Serial.printf("[AUDIO] connecttohost returned: %d\n", connected);
                    Serial.printf("[AUDIO] Heap after connect: %d | Largest block: %d\n",
                                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());

                    strncpy(current_url, audioData.url, sizeof(current_url) - 1);
                    current_url[sizeof(current_url) - 1] = '\0';
                    paused = false;

                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    Serial.printf("[AUDIO] audio.isRunning(): %d\n", audio->isRunning());
                }
                else
                {
                    Serial.println("[AUDIO] ERROR: Empty URL received!");
                }

                xEventGroupSetBits(taskEvents, AUDIO_STARTED_BIT);
                break;

            case CMD_STOP:
                Serial.println("Audio STOP commando ontvangen");
                audio->stopSong();
                current_url[0] = '\0';
                paused = false;
                break;

            case CMD_PAUSE:
                Serial.println("Audio PAUSE commando ontvangen");
                if (!paused)
                {
                    audio->pauseResume();
                    paused = true;
                    Serial.println("Audio is gepauzeerd");
                    xEventGroupSetBits(taskEvents, AUDIO_PAUSED_BIT);
                }
                break;

            case CMD_RESUME:
                Serial.println("Audio RESUME commando ontvangen");
                if (paused)
                {
                    audio->pauseResume();
                    paused = false;
                    Serial.println("Audio hervat");
                    xEventGroupSetBits(taskEvents, AUDIO_STARTED_BIT);
                }
                break;

            case CMD_SET_VOLUME:
                if (audioData.volume != current_volume)
                {
                    audio->setVolume(audioData.volume);
                    current_volume = audioData.volume;
                    Serial.printf("Volume aangepast naar: %d\n", current_volume);
                }
                break;

            default:
                break;
            }
        }

        audio->loop();

        bool isNowRunning = audio->isRunning();

        if (wasRunning && !isNowRunning && !paused)
        {
            Serial.println("Audio naar standby");
            xEventGroupSetBits(taskEvents, AUDIO_STOPPED_BIT);
        }

        wasRunning = isNowRunning;
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}