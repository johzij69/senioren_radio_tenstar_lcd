#include "Task_Audio.h"
#include "Audio.h"
#include "task_shared.h"

// DAC I2S

#define I2S_BCLK 5 // BCK
#define I2S_LRC 6 // LCK
#define I2S_DOUT 7 // DIN

#define VOLUME_STEPS 30
#define MIN_VOLUME 0
#define DEF_VOLUME 10

void AudioTask(void *parameter)
{

    Audio audio;
    String current_url = "";
    int current_volume = MIN_VOLUME;
    QueueHandle_t AudioQueue = static_cast<QueueHandle_t>(parameter);

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
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
        AudioData _audioData;
        if (xQueueReceive(AudioQueue, &_audioData, 0) == pdTRUE)
        {

            Serial.println("Data redceived for audio task");
            if (current_volume != _audioData.volume)
            {
                Serial.println("Volume changed to: " + String(_audioData.volume));
                audio.setVolume(_audioData.volume);
                current_volume = _audioData.volume;
            }

            const String streamUrl = String(_audioData.url);
            if (streamUrl != current_url)
            {
                Serial.println("Switching to stream: " + streamUrl);
                Serial.println("playing:" + streamUrl);
                audio.connecttohost(_audioData.url);
                current_url = streamUrl;
                //                myPrefs.writeString("lasturl", _audioData.url);
                //                myPrefs.putUInt("stream_index", streamIndex);
            }
        }
 //       Serial.println("playing audio");
        audio.loop();
        vTaskDelay(10);
    }
}
