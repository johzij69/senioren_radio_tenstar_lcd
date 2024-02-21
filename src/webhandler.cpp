#include "webhandler.h"

PrioWebServer::PrioWebServer(int port) : server(port) {
      PARAM_MESSAGE = "message";
}

void PrioWebServer::begin()
{

  
  // Route definities
  server.on("/", HTTP_GET, std::bind(&PrioWebServer::handleRoot, this, std::placeholders::_1));

  server.on("/updateurls", HTTP_POST, [](AsyncWebServerRequest *request)
            {
        String newurl;
        if (request->hasParam("newurl", true)) {
            newurl = request->getParam("newurl", true)->value();
        } else {
            newurl = "No message sent";
        }
        request->send(200, "text/plain", "Url toevoegen: " + newurl); });

  // Begin de server
  server.begin();
}

void PrioWebServer::handleRoot(AsyncWebServerRequest *request)
{
  String html = "<html><body>";
  html += "<form method='post' action='/updateurls'>";
  html += "Nieuwe URL-lijst: <input type='text' name='newurl'>";
  html += "<input type='submit' value='Bijwerken'>";
  html += "</form>";
  html += "</body></html>";

  request->send(200, "text/html", html);
}
