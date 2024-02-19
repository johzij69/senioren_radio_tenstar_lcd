#ifndef PLAY_H
#define PLAY_H

#include <Arduino.h>
#include <VS1053.h>
#include "WiFi.h"

class Play {
public:
  Play(int csPin, int dcsPin, int dreqPin);
  void playUrl(const char *url);
  void setVolume(int iVolume);
  void begin();
  void switchToMp3Mode();
  void stop();
  void play();
  void reset();
  void resetStream();

private:
  void splitUrl(const char *url, char *host, char *path, int &port);
  void playChunk(uint8_t *buffer, uint8_t length);

  WiFiClient client;
  uint8_t mp3buff[64];  // Adjust the buffer size as needed
  VS1053 player;
};

#endif // PLAY_H
