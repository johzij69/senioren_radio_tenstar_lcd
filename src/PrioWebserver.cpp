#include "PrioWebServer.h"

PrioWebServer::PrioWebServer(UrlManager& urlManager, MyPreferences& prefs, int port)
    : urlManager(urlManager), myPreferences(prefs), server(port)
{

}

void PrioWebServer::begin()
{

  // Route definities
  server.on("/", HTTP_GET, std::bind(&PrioWebServer::handleRoot, this, std::placeholders::_1));
  server.on("/showurls", HTTP_GET, [this](AsyncWebServerRequest *request)
            {
        Serial.println("getting urls from prferences:");
        urlManager.printAllUrls();
        

        String urlsAsString = urlManager.CreateDivUrls();
        Serial.println(urlsAsString);
        request->send(200, "text/html", urlsAsString); });

  server.on("/updateurls", HTTP_POST, [this](AsyncWebServerRequest *request)
            {
              String newurl;
              if (request->hasParam("newurl", true))
              {
                newurl = request->getParam("newurl", true)->value();
              }
              else
              {
                newurl = "No message sent";
              }
              request->send(200, "text/plain", "Url toevoegen: " + newurl);

               urlManager.addUrl(newurl.c_str()); });

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
