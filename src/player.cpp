#include "player.h"

Play::Play(int csPin, int dcsPin, int dreqPin) : player(csPin, dcsPin, dreqPin)
{
    // Other constructor initialization if needed
    loadUrls();
}

void Play::playUrl()
{

    if (!client.connected())
    {
        Serial.println("Reconnecting...");
        Serial.println(current_host);
        Serial.println(current_path);
        Serial.println(current_port);
        if (client.connect(current_host, current_port))
        {
            client.print(String("GET ") + current_path + " HTTP/1.0\r\n" +
                         "Host: " + current_host + "\r\n" +
                         "Connection: close\r\n\r\n");
        }
        else
        {
            Serial.print("Could not connect to ");
            Serial.println(current_url);
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

void Play::play(const char *url)
{
    splitUrl(url, current_host, current_path, current_port);
    playUrl();
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

void Play::reset()
{
    player.softReset();
}
void Play::resetStream()
{
    client.stop();
    client.flush();
}

void Play::updateUrls(const char *newUrls)
{
    // Update de lijst met URLs
    strcpy(path, newUrls);
    saveUrls();
}

void Play::saveUrls()
{
    // Sla de lijst met URLs op in Preferences
    Preferences preferences;
    preferences.begin(this->preferencesNamespace, false);
    preferences.putString("urls", path);
    preferences.end();
}

void Play::loadUrls()
{
    // Laad de lijst met URLs uit Preferences
    Preferences preferences;
    preferences.begin(this->preferencesNamespace, true);
    String savedUrls = preferences.getString("urls", "");
    strcpy(path, savedUrls.c_str());
    preferences.end();
}