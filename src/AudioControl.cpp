#include "AudioControl.h"
#include "Task_Shared.h"
#include "globals.h"
#include "Task_Audio.h"  // Voor CMD_PLAY etc.
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include <WiFiClientSecure.h>

extern QueueHandle_t AudioQueue;
extern EventGroupHandle_t taskEvents;

static String resolveStreamUrl(const char *url)
{
    if (url == nullptr || url[0] == '\0')
    {
        return String();
    }

    String inputUrl(url);
    if (!inputUrl.startsWith("http://") && !inputUrl.startsWith("https://"))
    {
        return inputUrl;
    }

    bool isHttps = inputUrl.startsWith("https://");
    int schemeLength = isHttps ? 8 : 7;
    int hostStart = schemeLength;
    int pathStart = inputUrl.indexOf('/', hostStart);
    String hostPort = pathStart >= 0 ? inputUrl.substring(hostStart, pathStart) : inputUrl.substring(hostStart);
    String path = pathStart >= 0 ? inputUrl.substring(pathStart) : String("/");
    int port = isHttps ? 443 : 80;

    int colonPos = hostPort.indexOf(':');
    if (colonPos >= 0)
    {
        port = hostPort.substring(colonPos + 1).toInt();
        hostPort = hostPort.substring(0, colonPos);
    }

    String request = String("HEAD ") + path + " HTTP/1.1\r\n" +
                     "Host: " + hostPort + "\r\n" +
                     "User-Agent: Mozilla/5.0\r\n" +
                     "Accept: */*\r\n" +
                     "Connection: close\r\n\r\n";

    String location;

    if (isHttps)
    {
        WiFiClientSecure client;
        client.setInsecure();
        client.setHandshakeTimeout(10);
        client.setTimeout(3000);

        if (!client.connect(hostPort.c_str(), port))
        {
            return inputUrl;
        }

        client.print(request);

        String statusLine = client.readStringUntil('\n');
        statusLine.trim();
        if (!statusLine.startsWith("HTTP/"))
        {
            client.stop();
            return inputUrl;
        }

        while (client.connected())
        {
            String line = client.readStringUntil('\n');
            line.trim();
            if (line.length() == 0)
            {
                break;
            }

            if (line.startsWith("Location:") || line.startsWith("location:"))
            {
                int colon = line.indexOf(':');
                if (colon >= 0)
                {
                    location = line.substring(colon + 1);
                    location.trim();
                }
            }
        }

        client.stop();
    }
    else
    {
        WiFiClient client;
        client.setTimeout(3000);

        if (!client.connect(hostPort.c_str(), port))
        {
            return inputUrl;
        }

        client.print(request);

        String statusLine = client.readStringUntil('\n');
        statusLine.trim();
        if (!statusLine.startsWith("HTTP/"))
        {
            client.stop();
            return inputUrl;
        }

        while (client.connected())
        {
            String line = client.readStringUntil('\n');
            line.trim();
            if (line.length() == 0)
            {
                break;
            }

            if (line.startsWith("Location:") || line.startsWith("location:"))
            {
                int colon = line.indexOf(':');
                if (colon >= 0)
                {
                    location = line.substring(colon + 1);
                    location.trim();
                }
            }
        }

        client.stop();
    }

    if (location.length() == 0)
    {
        return inputUrl;
    }

    if (location.startsWith("http://") || location.startsWith("https://"))
    {
        return location;
    }

    if (location.startsWith("//"))
    {
        return String(isHttps ? "https:" : "http:") + location;
    }

    if (location.startsWith("/"))
    {
        return String(isHttps ? "https://" : "http://") + hostPort + location;
    }

    return inputUrl;
}

void playAudio(const char *url)
{
    String resolvedUrl = resolveStreamUrl(url);

    if (resolvedUrl.length() == 0)
    {
        resolvedUrl = String(url == nullptr ? "" : url);
    }
    
    //todo
    AudioData data;
    data.command = CMD_PLAY;
 //   data.volume = volume;
    data.volume = last_volume;
    strncpy(data.url, resolvedUrl.c_str(), sizeof(data.url));
    data.url[sizeof(data.url) - 1] = '\0';
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
    data.command = CMD_SET_VOLUME;
    data.volume = volume;
    data.url[0] = '\0';
    xQueueSend(AudioQueue, &data, portMAX_DELAY);
}


