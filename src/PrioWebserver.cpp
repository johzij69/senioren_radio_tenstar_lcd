#include "PrioWebServer.h"

PrioWebServer::PrioWebServer(UrlManager &urlManager, int port)
    : urlManager(urlManager), server(port)
{
}

void PrioWebServer::begin()
{

  // Route definities
  server.on("/", HTTP_GET, std::bind(&PrioWebServer::handleRoot, this, std::placeholders::_1));
  server.on("/addurl", HTTP_GET, std::bind(&PrioWebServer::handleAddUrl, this, std::placeholders::_1));

  server.on("/deleteurl", HTTP_GET, [this](AsyncWebServerRequest *request)
            { this->deleteStreamItem(request); });

  server.on("/deleteurl", HTTP_POST, [this](AsyncWebServerRequest *request)
            {
    String index;
    if (request->hasParam("urlindex", true))
    {
      index = request->getParam("urlindex", true)->value();
      urlManager.deleteUrl(index.toInt());
      request->redirect("/showurls");
    }
    else
    {
      request->send(200, "text/plain", "Url not found");
    } });
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

void PrioWebServer::handleAddUrl(AsyncWebServerRequest *request)
{
  String body = "";
  body += "<form method='post' action='/updateurls'>";
  body += "Nieuwe URL-lijst: <input type='text' name='newurl'>";
  body += "<input type='submit' value='Bijwerken'>";
  body += "</form>";
  request->send(200, "text/html", createHtmlPage(body));
}

void PrioWebServer::handleRoot(AsyncWebServerRequest *request)
{

  String body = "";
  for (size_t i = 0; i < urlManager.urls.size(); i++)
  {
    body += "<div id=\"" + String(i) + "\"><p>URL " + String(i) + ": " + urlManager.urls[i] + "<p><div>";
  }

  request->send(200, "text/html", createHtmlPage(body));
}

void PrioWebServer::deleteStreamItem(AsyncWebServerRequest *request)
{

  String body = "";
  for (size_t i = 0; i < urlManager.urls.size(); i++)
  {
    body += "<form action='deleteurl' method='post'>";
    body += "<p>";
    body += "<label for='url'" + String(i) + ">URL" + String(i) + ":</label>";
    body += "<span>";
    body += urlManager.urls[i];
    body += "</span>";
    body += "<button type='submit' name='urlindex' value='" + String(i) + "'>Verwijder</button>";
    body += "</p>";
    body += "</form>";
  }

  request->send(200, "text/html", createHtmlPage(body));
}

String PrioWebServer::createHtmlPage(String html)
{
  String page = getHtmlStart();
  page += html;
  page += getHtmlEnd();
  return page;
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
  html += "<title>Senior Webradio</title>";
  html += "<style>";
  html += getStyling();
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += getTopMenu();
  return html;
}

String PrioWebServer::getHtmlEnd()
{
  String endHtml = "</body>";
  endHtml += "</html>";
  return endHtml;
}

String PrioWebServer::getTopMenu()
{

  String html = "";
  html += "<div class='top-menu'>";
  html += "<a href='/'>Overzicht</a>";
  html += "<a href='/deleteurl'>Bewerk stations</a>";
  html += "<a href='#'>Services</a>";
  html += "<a href='#'>Contact</a>";
  html += "</div>";

  return html;
}

String PrioWebServer::getStyling()
{
  String Style = "";
  Style += "body { margin: 0; font-family: Arial, sans-serif;}";
  Style += ".top-menu { background-color: #333; overflow: hidden;}";
  Style += ".top-menu a { float: left; display: block; color: white; text-align: center; padding: 14px 16px; text-decoration: none;}";
  Style += ".top-menu a:hover { background-color: #ddd; color: black;}";
  Style += "@media screen and (max-width: 600px)";
  Style += "{.top-menu a { float: none; display: block; width: 100%; text-align: left;}}";

  return Style;
}