#include <task_audio.h>
#include "audioPlayerDefaults.h"

// DAC I2S
#define I2S_DOUT 7
#define I2S_BCLK 5
#define I2S_LRC 6

    Audio audio;
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

void AudioTask(void *parameter)
{



  //  audio_Defaults *audioDefaults = (audio_Defaults *)parameter;
  //  audio.setVolumeSteps(audioDefaults->volume_steps);
  //  audio.setVolume(audioDefaults->max_Volume);


    AudioQueues *AuudioQueue = (AudioQueues*)parameter;
    QueueHandle_t audioStreamQueue = AuudioQueue->StreamQ;
    QueueHandle_t audioVolumeQueue =   AuudioQueue->VolumeQ;

    // audioStreamQueue = xQueueCreate(3, sizeof(String));
    // audioVolumeQueue = xQueueCreate(3, sizeof(int));

    while (true)
    {

        String sUrl;
        int iVolume;

        if (xQueueReceive(audioVolumeQueue, &iVolume, 0) == pdTRUE)
        {
            audio.setVolume(iVolume);
        }

        if (xQueueReceive(audioStreamQueue, &sUrl, 0) == pdTRUE)
        {

            Serial.println("Switching stream! ");

            //
            // const String streamUrl = UrlManagerInstance.Streams[streamIndex].url;

            if (sUrl != "")
            {
                // Serial.println("Switching to stream: " + streamUrl);
                // Serial.print("stream count: ");
                // Serial.println(UrlManagerInstance.streamCount);
                // Serial.print("stream index: ");
                // Serial.println(streamIndex);
                // Serial.println("playing:");
                // Serial.println(current_url);
                // Serial.print("stream logo: ");
                // Serial.println(UrlManagerInstance.Streams[streamIndex].logo);

                audio.connecttohost(sUrl.c_str());
                //                current_url = streamUrl.c_str();
                //                myPrefs.writeString("lasturl", streamUrl.c_str());
                //                myPrefs.putUInt("stream_index", streamIndex);
            }
        }

        audio.loop();
    }
}

// optional
void audio_info(const char *info)
{
    Serial.print("info        ");
    Serial.println(info);
}
void audio_id3data(const char *info)
{ // id3 metadata
    Serial.print("id3data     ");
    Serial.println(info);
}
void audio_eof_mp3(const char *info)
{ // end of file
    Serial.print("eof_mp3     ");
    Serial.println(info);
}
void audio_showstation(const char *info)
{
    Serial.print("station     ");
    Serial.println(info);
}
void audio_showstreamtitle(const char *info)
{
    Serial.print("streamtitle ");
    Serial.println(info);
    //  prioTft.setTitle(info);
    // streamSwitched = true;
}
void audio_bitrate(const char *info)
{
    Serial.print("bitrate     ");
    Serial.println(info);
}
void audio_commercial(const char *info)
{ // duration in sec
    Serial.print("commercial  ");
    Serial.println(info);
}
void audio_icyurl(const char *info)
{ // homepage
    Serial.print("icyurl      ");
    Serial.println(info);
}
void audio_lasthost(const char *info)
{ // stream URL played
    Serial.print("lasthost    ");
    Serial.println(info);
}
void audio_eof_speech(const char *info)
{
    Serial.print("eof_speech  ");
    Serial.println(info);
}