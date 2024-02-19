#include "player.h"

Play::Play(int csPin, int dcsPin, int dreqPin) : player(csPin, dcsPin, dreqPin)
{
    // Other constructor initialization if needed
}

void Play::playUrl(const char *url)
{
    char host[100];
    char path[200];
    int port;

    splitUrl(url, host, path, port);

   
    if (!client.connected())
    {
        Serial.println("Reconnecting...");
        if (client.connect(host, port))
        {
            client.print(String("GET ") + path + " HTTP/1.0\r\n" +
                         "Host: " + host + "\r\n" +
                         "Connection: close\r\n\r\n");
        }
        else
        {
            Serial.print("Could not connect to ");
            Serial.println(url);
            return;
        }
    }

    if (client.available() > 0)
    {
        // The buffer size 64 seems to be optimal. At 32 and 128 the sound might be brassy.
        uint8_t bytesread = client.read(mp3buff, 64);
        playChunk(mp3buff, bytesread);
    }
}

void Play::splitUrl(const char *url, char *host, char *path, int &port)
{
    // Parse the URL to extract host, path, and port
    sscanf(url, "https://%99[^/]/%199[^:]:%d", host, path, &port);
    // Voeg "//" toe aan het begin van het pad
    String fullPath = "//" + String(path);
    strcpy(path, fullPath.c_str());
}

void Play::playChunk(uint8_t *buffer, uint8_t length)
{
    player.playChunk(buffer, length);
}
void Play::setVolume(int iVolume)
{
    player.setVolume(iVolume);
}
void Play::begin()
{
    player.begin();
}
void Play::switchToMp3Mode()
{
    player.switchToMp3Mode();
}
void Play::stop()
{
    player.stopSong();
}
void Play::play()
{
    player.stopSong();
}
void Play::reset()
{
    player.softReset();
}
void Play::resetStream()
{
    client.stop();
    client.flush();
}