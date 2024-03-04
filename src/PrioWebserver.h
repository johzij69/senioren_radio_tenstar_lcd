#ifndef PrioWebServer_H
#define PrioWebServer_H

#include <ESPAsyncWebServer.h>
#include "UrlManager.h"
#include "favicon.h"

class PrioWebServer
{
public:
  PrioWebServer(UrlManager &urlManager, int port);

  void begin();

private:
  AsyncWebServer server;
  UrlManager &urlManager;

  String getHtmlStart();
  String getHtmlEnd();
  String getTopMenu();
  String getStyling();
  String createHtmlPage(String html);
  void handleRoot(AsyncWebServerRequest *request);
  void handleAddUrl(AsyncWebServerRequest *request);
  void deleteStreamItem(AsyncWebServerRequest *request);
};

#endif
