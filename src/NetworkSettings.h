#pragma once

#include <Arduino.h>

namespace NetworkSettings
{
bool connectPreferred();
void connectionReady();
void poll();
bool requestScan();
bool selectAccessPoint(const String &bssid, uint32_t timeoutSeconds);
bool getStatus(String &json);
void reset();
}