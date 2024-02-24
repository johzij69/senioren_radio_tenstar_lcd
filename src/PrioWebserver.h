#ifndef webhandler_h
#define webhandler_h

#include <ESPAsyncWebServer.h>
// #include "AnotherClass.h"
#include "UrlManager.h"

class PrioWebServer
{
public:
  PrioWebServer(UrlManager &urlManager, MyPreferences &prefs, int port);

  void begin();

private:
  AsyncWebServer server;
  UrlManager &urlManager;
  MyPreferences &myPreferences;

  void handleRoot(AsyncWebServerRequest *request);
};

#endif
