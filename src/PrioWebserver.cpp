#include "PrioWebServer.h"

PrioWebServer::PrioWebServer(UrlManager &urlManager, int port)
    : urlManager(urlManager), server(port)
{
}

void PrioWebServer::begin()
{

  // Route definities
  server.on("/", HTTP_GET, std::bind(&PrioWebServer::handleRoot, this, std::placeholders::_1));

  server.on("/deleteurl", HTTP_GET, [this](AsyncWebServerRequest *request)
            {
        this->deleteStreamItem(request);
         });

  server.on("/deleteurl", HTTP_POST, [this](AsyncWebServerRequest *request)
            {
              String index;
              if (request->hasParam("urlindex", true))
              {
                index = request->getParam("urlindex", true)->value();
                urlManager.deleteUrl(index.toInt() );
              }
              else
              {
                index = "-1";
              }
this->deleteStreamItem(request);
               });
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
  server.on("/showprefs", HTTP_GET, [this](AsyncWebServerRequest *request)
            {
        Serial.println("getting urls from prferences:");
        urlManager.PrinturlsFromPrev();
        request->send(200, "text/html", "see console"); });

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

void PrioWebServer::deleteStreamItem(AsyncWebServerRequest *request)
{

  String html = getHtmlStart();
  String body = "<body";
  for (size_t i = 0; i < urlManager.urls.size(); i++)
  {
    body += "<form action='deleteurl' method='post'>";
    body += "<p>";
    body += "<label for='url'" + String(i) + ">URL " + String(i) + ":</label>";
    body += "<span>";
    body += urlManager.urls[i];
    body += "</span>";
    body += "<button type='submit' name='urlindex' value='" + String(i) + "'>Verwijder</button>";
    body += "</p>";
    body += "</form>";
  }

  body += "</body>";
  html += body;
  html += "<html>";
  request->send(200, "text/html", html);
}

String PrioWebServer::getHtmlStart()
{

  String html = "";
  html += "<!DOCTYPE html>";
  html += "<html lang='nl'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta http-equiv='X-UA-Compatible' content='IE=edge'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>";
  html += "<title>URL Lijst</title>";
  html += "<style>";
  html += "/* Voeg hier eventueel wat stijling toe voor opmaak */";
  html += "</style>";
  html += "</head>";

  return html;
}
