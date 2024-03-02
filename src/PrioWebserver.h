#ifndef PrioWebServer_H
#define PrioWebServer_H

#include <ESPAsyncWebServer.h>
#include "UrlManager.h"

class PrioWebServer
{
public:
  PrioWebServer(UrlManager &urlManager, int port);

  void begin();

private:
  AsyncWebServer server;
  UrlManager &urlManager;

  String getHtmlStart();
  void handleRoot(AsyncWebServerRequest *request);
  void deleteStreamItem(AsyncWebServerRequest *request);
};

#endif
