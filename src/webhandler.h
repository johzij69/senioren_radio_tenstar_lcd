#ifndef webhandler_h
#define webhandler_h

#include <ESPAsyncWebServer.h>

class PrioWebServer
{
public:
  PrioWebServer(int port);

  void begin();

private:
  AsyncWebServer server;

  const char *PARAM_MESSAGE;

  void handleRoot(AsyncWebServerRequest *request);
};

#endif
