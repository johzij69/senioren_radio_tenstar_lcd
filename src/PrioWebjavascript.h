#ifndef PrioWebjavascript_H
#define PrioWebjavascript_H
#include <Arduino.h>
#include "generalHelpers.h"

String getMainScript(String ip);
String getAddScript(String ip);
String getAlarmScript(String ip);
String getSettingsScript(String ip, int snoozeButtonIndex, int sleepMinutes, int backlightMinPercent, int backlightMaxPercent);
String getContactScript(String ip);
String getImportExportScript(String ip);

#endif