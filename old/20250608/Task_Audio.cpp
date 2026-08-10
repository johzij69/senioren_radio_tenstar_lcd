#include "Task_Audio.h"
#include "Audio.h"
#include "Task_Shared.h"
#include "globals.h"

// ============================================================================
// GLOBAAL HERGEBRUIKTE SSL CLIENT voor redirect resolving
// ============================================================================
static WiFiClientSecure *redirectClient = nullptr;

static void initRedirectClient()
{
    if (redirectClient == nullptr)
    {
        redirectClient = new WiFiClientSecure();
        redirectClient->setInsecure();
        redirectClient->setHandshakeTimeout(10);
        redirectClient->setTimeout(3000);
        // setBufferSizes() bestaat NIET in ESP32 core 3.x — verwijderd!
    }
    redirectClient->stop();
}

// ============================================================================
// Redirect resolver zonder String class (alleen char buffers)
// ============================================================================
static bool startsWithIgnoreCase(const char *str, const char *prefix)
{
    while (*prefix)
    {
        if (tolower((unsigned char)*str++) != tolower((unsigned char)*prefix++))
            return false;
    }
    return true;
}

//Maak de buffers in resolveStreamUrl() static zodat ze in het .data/.bss segment komen in plaats van op de task stack. Dit bespaart ~1.5 KB stack per aanroep.
// Vervang de lokale declaraties in resolveStreamUrl() door static:

static bool resolveStreamUrl(const char *inputUrl, char *out, size_t outLen)
{
    if (!inputUrl || !inputUrl[0] || !out || !outLen) return false;

    static char url[256];
    strncpy(url, inputUrl, sizeof(url) - 1);
    url[sizeof(url) - 1] = '\0';

    bool isHttps = (strncmp(url, "https://", 8) == 0);
    bool isHttp  = (strncmp(url, "http://",  7) == 0);

    if (!isHttps && !isHttp)
    {
        strncpy(out, url, outLen - 1);
        out[outLen - 1] = '\0';
        return true;
    }

    int schemeLen = isHttps ? 8 : 7;
    char *hostPort = url + schemeLen;
    char *pathPtr  = strchr(hostPort, '/');

    static char host[128];
    static char path[128] = "/";

    if (pathPtr)
    {
        size_t hlen = pathPtr - hostPort;
        if (hlen >= sizeof(host)) hlen = sizeof(host) - 1;
        strncpy(host, hostPort, hlen);
        host[hlen] = '\0';
        strncpy(path, pathPtr, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }
    else
    {
        strncpy(host, hostPort, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
    }

    int port = isHttps ? 443 : 80;
    char *colon = strchr(host, ':');
    if (colon)
    {
        *colon = '\0';
        port = atoi(colon + 1);
    }

    static char request[512];
    snprintf(request, sizeof(request),
             "HEAD %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: Mozilla/5.0\r\n"
             "Accept: */*\r\n"
             "Connection: close\r\n\r\n",
             path, host);

    static char location[256];
    location[0] = '\0';

    if (isHttps)
    {
        initRedirectClient();
        if (!redirectClient->connect(host, port))
        {
            strncpy(out, inputUrl, outLen - 1);
            out[outLen - 1] = '\0';
            return true;
        }
        redirectClient->print(request);

        static char line[128];
        int n = redirectClient->readBytesUntil('\n', line, sizeof(line) - 1);
        if (n > 0)
        {
            line[n] = '\0';
            if (n > 0 && line[n - 1] == '\r') line[n - 1] = '\0';
        }

        if (strncmp(line, "HTTP/", 5) != 0)
        {
            redirectClient->stop();
            strncpy(out, inputUrl, outLen - 1);
            out[outLen - 1] = '\0';
            return true;
        }

        while (redirectClient->connected())
        {
            n = redirectClient->readBytesUntil('\n', line, sizeof(line) - 1);
            if (n <= 0) break;
            line[n] = '\0';
            if (n > 0 && line[n - 1] == '\r') line[n - 1] = '\0';

            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0') break;

            if (startsWithIgnoreCase(p, "location:"))
            {
                char *loc = p + 9;
                while (*loc == ' ' || *loc == '\t') loc++;
                strncpy(location, loc, sizeof(location) - 1);
                location[sizeof(location) - 1] = '\0';
            }
        }
        redirectClient->stop();
    }
    else
    {
        WiFiClient client;
        client.setTimeout(3000);
        if (!client.connect(host, port))
        {
            strncpy(out, inputUrl, outLen - 1);
            out[outLen - 1] = '\0';
            return true;
        }
        client.print(request);

        static char line[128];
        int n = client.readBytesUntil('\n', line, sizeof(line) - 1);
        if (n > 0)
        {
            line[n] = '\0';
            if (line[n - 1] == '\r') line[n - 1] = '\0';
        }

        if (strncmp(line, "HTTP/", 5) != 0)
        {
            client.stop();
            strncpy(out, inputUrl, outLen - 1);
            out[outLen - 1] = '\0';
            return true;
        }

        while (client.connected())
        {
            n = client.readBytesUntil('\n', line, sizeof(line) - 1);
            if (n <= 0) break;
            line[n] = '\0';
            if (line[n - 1] == '\r') line[n - 1] = '\0';

            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0') break;

            if (startsWithIgnoreCase(p, "location:"))
            {
                char *loc = p + 9;
                while (*loc == ' ' || *loc == '\t') loc++;
                strncpy(location, loc, sizeof(location) - 1);
                location[sizeof(location) - 1] = '\0';
            }
        }
        client.stop();
    }

    if (location[0] == '\0')
    {
        strncpy(out, inputUrl, outLen - 1);
        out[outLen - 1] = '\0';
        return true;
    }

    if (strncmp(location, "http://", 7) == 0 || strncmp(location, "https://", 8) == 0)
    {
        strncpy(out, location, outLen - 1);
        out[outLen - 1] = '\0';
    }
    else if (strncmp(location, "//", 2) == 0)
    {
        snprintf(out, outLen, "%s%s", isHttps ? "https:" : "http:", location);
    }
    else if (location[0] == '/')
    {
        snprintf(out, outLen, "%s%s%s", isHttps ? "https://" : "http://", host, location);
    }
    else
    {
        snprintf(out, outLen, "%s%s/%s", isHttps ? "https://" : "http://", host, location);
    }
    return true;
}

// ============================================================================
// AudioTask
// ============================================================================
void AudioTask(void *parameter)
{
    Audio *audio = new Audio(); 
    char current_url[256] = "";
    int current_volume = MIN_VOLUME;
    bool paused = false;
    bool wasRunning = false;

    QueueHandle_t AudioQueue = static_cast<QueueHandle_t>(parameter);

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

                if (audioData.volume != current_volume)
                {
                    audio->setVolume(audioData.volume);
                    current_volume = audioData.volume;
                    Serial.printf("Volume aangepast naar: %d\n", current_volume);
                }

                if (audioData.url[0] != '\0')
                {
                    Serial.println("[AUDIO] Starting/restarting stream");
                    Serial.printf("[AUDIO] Heap: %d | Largest block: %d\n",
                                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());

                    if (current_url[0] != '\0')
                    {
                        Serial.println("[AUDIO] Stopping current stream first");
                        audio->stopSong();
                        vTaskDelay(500 / portTICK_PERIOD_MS);
                    }

                    // RESOLVE REDIRECT HIER, in de AudioTask, vlak voor connecten
                    char resolvedUrl[256];
                    if (!resolveStreamUrl(audioData.url, resolvedUrl, sizeof(resolvedUrl)))
                    {
                        strncpy(resolvedUrl, audioData.url, sizeof(resolvedUrl) - 1);
                        resolvedUrl[sizeof(resolvedUrl) - 1] = '\0';
                    }

                    Serial.printf("[AUDIO] Resolved URL: %s\n", resolvedUrl);
                    Serial.printf("[AUDIO] Heap before connect: %d | Largest block: %d\n",
                                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());

                    bool connected = audio->connecttohost(resolvedUrl);
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

        bool isNowRunning = audio-> isRunning();

        if (wasRunning && !isNowRunning && !paused)
        {
            Serial.println("Audio naar standby");
            xEventGroupSetBits(taskEvents, AUDIO_STOPPED_BIT);
        }

        wasRunning = isNowRunning;
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}