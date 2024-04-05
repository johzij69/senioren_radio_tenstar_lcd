#ifndef PrioWebServer_H
#define PrioWebServer_H

#include <ESPAsyncWebServer.h>
#include "UrlManager.h"
#include "favicon.h"
#include "ArduinoJson.h"
#include "PrioWebjavascript.h"
#include "PrioWebcss.h"

class PrioWebServer
{
public:
  PrioWebServer(UrlManager &urlManager, int port);

  void begin();
  String ip;
  

private:
  AsyncWebServer server;
  UrlManager &urlManager;

  String htmlPage;

  String PROGMEM bigString;

  String getHtmlStart();
  String getHtmlEnd();
  String getTopMenu();
  String setHtmlBody(String body);


  String createHtmlPage(String body);
  void handleRoot(AsyncWebServerRequest *request);
  void handleAddUrl(AsyncWebServerRequest *request);
  void handleAddStream(AsyncWebServerRequest *request, uint8_t *data);
  void handleInputStream(AsyncWebServerRequest *request);
  void deleteStreamItem(AsyncWebServerRequest *request);
  void handleSettings(AsyncWebServerRequest *request, uint8_t *data);



  void handleApi(AsyncWebServerRequest *request);
};

#endif
