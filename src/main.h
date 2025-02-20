#include "Arduino.h"
#include <SPI.h>
#include <TFT_eSPI.h>

void CreateAndSendDisplayData(int streamIndex);
void CreateAndSendAudioData(int streamIndex, int last_volume);
void oldloop();