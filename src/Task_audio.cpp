#include "Task_Audio.h"
#include "Audio.h"

// DAC I2S
#define I2S_DOUT 7
#define I2S_BCLK 5
#define I2S_LRC 6

#define VOLUME_STEPS 30
#define MIN_VOLUME 0
#define DEF_VOLUME 10

void AudioTask(void *parameter)
{

    Audio audio;

    QueueHandle_t AudioQueue = static_cast<QueueHandle_t>(parameter);
    int current_volume = MIN_VOLUME;
    String current_url = "";

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    // audio.setAudioInfoCallback(audio_info);
    // audio.setAudioId3MetadataCallback(audio_id3data);
    // audio.setAudioEOFCallback(audio_eof_mp3);
    // audio.setAudioShowStationCallback(audio_showstation);
    // audio.setAudioShowStreamTitleCallback(audio_showstreamtitle);
    // audio.setAudioBitrateCallback(audio_bitrate);
    // audio.setAudioCommercialCallback(audio_commercial);
    // audio.setAudioIcyUrlCallback(audio_icyurl);

    audio.setVolume(DEF_VOLUME);
    audio.setVolumeSteps(VOLUME_STEPS);

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
                audio.stopSong(); // Zorgt ervoor dat de vorige stream netjes wordt gesloten
//                delay(100);       // Eventueel kleine vertraging om resource vrij te geven
                vTaskDelay(1);
                audio.connecttohost(_audioData.url);
                current_url = streamUrl;
                //                myPrefs.writeString("lasturl", _audioData.url);
                //                myPrefs.putUInt("stream_index", streamIndex);
            }
        }
        audio.loop();
        vTaskDelay(1);
    }
}
