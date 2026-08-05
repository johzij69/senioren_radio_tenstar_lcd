#ifndef PrioWebServer_H
#define PrioWebServer_H

#include <ESPAsyncWebServer.h>
#include "UrlManager.h"
#include "favicon.h"
#include "ArduinoJson.h"
#include "PrioWebjavascript.h"
#include "PrioWebcss.h"
#include "globals.h" 
#include "AlarmManager.h"
#include "MyPreferences.h"
#include "LittleFS.h"
#include "fetchImage.h"



class PrioWebServer
{
public:
  PrioWebServer(UrlManager &urlManager, AlarmManager &alarmManager, MyPreferences &preferences, int port);

  void begin();
  String ip;
  

private:
  AsyncWebServer server;
  UrlManager &urlManager;
  AlarmManager &alarmManager;
  MyPreferences &preferences;

  String htmlPage;

  String PROGMEM bigString;

  String getHtmlStart();
  String getHtmlEnd();
  String getTopMenu();
  String setHtmlBody(String body, String Script);
  String createHtmlPage(String body);

  void handleRoot(AsyncWebServerRequest *request);
  void handleAddStream(AsyncWebServerRequest *request, uint8_t *data);
  void handleInputStream(AsyncWebServerRequest *request);
  void handleSettings(AsyncWebServerRequest *request, uint8_t *data);//, size_t len);
  void handleDeleteStream(AsyncWebServerRequest *request);
  void handleInstellingen(AsyncWebServerRequest *request);
  void handleImportExportPage(AsyncWebServerRequest *request);
  void handleSynctime(AsyncWebServerRequest *request);
  void handleAlarmPage(AsyncWebServerRequest *request);
  void handleApiAlarms(AsyncWebServerRequest *request);
  void handleApiAlarmStatus(AsyncWebServerRequest *request);
  void handleApiExportConfig(AsyncWebServerRequest *request);
  void handleApiImportConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len);
  void handleSaveAlarms(AsyncWebServerRequest *request, uint8_t *data, size_t len);
  void handleApiSettings(AsyncWebServerRequest *request);
  void handleSaveSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len);
  void handleUploadLogo(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
  void handleUploadLogoReplace(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
  void handleRefreshLogo(AsyncWebServerRequest *request, uint8_t *data, size_t len);
  void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);




  void handleApi(AsyncWebServerRequest *request);
};

#endif
