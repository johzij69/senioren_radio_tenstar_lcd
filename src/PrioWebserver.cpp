#include "PrioWebServer.h"

PrioWebServer::PrioWebServer(UrlManager &urlManager, int port)
    : urlManager(urlManager), server(port)
{
}

void PrioWebServer::begin()
{

  /* CORS */
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  server.onNotFound([this](AsyncWebServerRequest *request)
                    {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404, "application/json", "{\"message\":\"Not found\"}");
    } });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "image/x-icon", (const unsigned char *)favicon, sizeof(favicon)); });

  /* root page , which handles overzicht */
  server.on("/", HTTP_GET, std::bind(&PrioWebServer::handleRoot, this, std::placeholders::_1));

    /* instellingen page , which handles instellingen */
    server.on("/instellingen", HTTP_GET, std::bind(&PrioWebServer::handleInstellingen, this, std::placeholders::_1));

  /* deliver the streams in json format */
  server.on("/api/streams", HTTP_GET, std::bind(&PrioWebServer::handleApi, this, std::placeholders::_1));

  server.on("/api/deletestream", HTTP_GET, std::bind(&PrioWebServer::handleDeleteStream, this, std::placeholders::_1));

  server.on("/api/synctime", HTTP_GET, std::bind(&PrioWebServer::handleSynctime, this, std::placeholders::_1));

  /* serves the html page to add a stream */
  server.on("/inpustream", HTTP_GET, std::bind(&PrioWebServer::handleInputStream, this, std::placeholders::_1));

      // Voeg een handler toe voor POST-verzoeken naar /updateurls
      server.on(
        "/updateurls",
        HTTP_POST,
        [](AsyncWebServerRequest * request){},
        NULL,
        [this](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
     
          for (size_t i = 0; i < len; i++) {
            Serial.write(data[i]);
          }
     
          Serial.println();
          this->handleSettings(request, data);
          request->send(200);
      });

      server.on(
        "/api/addstream",
        HTTP_POST,
        [](AsyncWebServerRequest * request){},
        NULL,
        [this](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
          urlManager.addStream(data);
          request->send(200);
      });
     
  server.begin();
}
void PrioWebServer::onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  // Handle body

  if (request->url() == "/updateurls")
  {

    Serial.println("Received body data");
    urlManager.saveStreams(data);
  }
}
void PrioWebServer::handleApi(AsyncWebServerRequest *request)
{

  int default_page_size = 5;
  int page_size = default_page_size;
  int page = 1;
  int start = 0;
  int end = 4;


  int total_pages = ceil((float)urlManager.streamCount / (float)page_size); // set default
  int paramsNr = request->params();

  for (int i = 0; i < paramsNr; i++)
  {
    const AsyncWebParameter *p = request->getParam(i);

    if (p->name() == "size")
    {
      if(p->value() != "all"){
        page_size = p->value().toInt();
      } else {
        page_size = urlManager.streamCount;
      } 
    }

    if (p->name() == "page")
    {
      page = p->value().toInt();
    }
  }

  /* determine page_size */
  if (page_size < 1 || page_size > urlManager.streamCount)
  {
    page_size = default_page_size;
  }

  if(page_size > urlManager.streamCount){
    page_size = urlManager.streamCount;
  }

  /* determine current page */
  total_pages = ceil((float)urlManager.streamCount / (float)page_size);
  if (page < 1)
  {
    page = 1;
  }
  if (page > total_pages)
  {
    page = total_pages;
  }

  /* Determine start and end stream id*/
  start = (page - 1) * page_size;
  end = start + (page_size - 1);

  if (end > urlManager.streamCount - 1)
  {
    end = urlManager.streamCount - 1;
  }

  /* Create json return object*/
  JsonDocument doc;
  doc["page"] = page;
  doc["streams_per_page"] = page_size;
  doc["total_streams"] = urlManager.streamCount;
  doc["total_pages"] = total_pages;
  doc["start_stream"] = start;
  doc["end_stream"] = end;

  JsonArray data = doc["data"].to<JsonArray>();

  for (size_t i = start; i <= end; i++)
  {
    JsonObject stream = data.add<JsonObject>();
    stream["id"] = i;
    stream["name"] = urlManager.Streams[i].name;
    stream["url"] = urlManager.Streams[i].url;
    stream["logo"] = urlManager.Streams[i].logo;
  }

  /* create response and send it*/
  String response;

  doc.shrinkToFit(); // optional

  serializeJson(doc, response);

  request->send(200, "application/json", response);
}

void PrioWebServer::handleDeleteStream(AsyncWebServerRequest *request)
{

  int paramsNr = request->params();

  for (int i = 0; i < paramsNr; i++)
  {
    const AsyncWebParameter *p = request->getParam(i);

    if (p->name() == "id")
    {
      urlManager.deleteStream(p->value().toInt());
    }
  }
  request->send(200, "text/plain", "ok");
}

void PrioWebServer::handleRoot(AsyncWebServerRequest *request)
{
  String body PROGMEM = R"(<div id="content-container" class="content"></div>)";
  String mybigString = "";

  String h_start PROGMEM = getHtmlStart();
  String h_script PROGMEM = getMainScript(this->ip);
  String h_body PROGMEM = setHtmlBody(body, h_script);
  String h_end PROGMEM = getHtmlEnd();

  mybigString.concat(h_start);
  mybigString.concat(h_body);
  mybigString.concat(h_end);

  request->send(200, "text/html", mybigString.c_str());
  mybigString = "";
}

void PrioWebServer::handleInstellingen(AsyncWebServerRequest *request)
{
 //TODO!!!!!
 
  String body PROGMEM = R"(<div id="content-container" class="content"></div>)";
   String mybigString = "";

  // String h_start PROGMEM = getHtmlStart();
  // String h_script PROGMEM = getMainScript(this->ip);
  // String h_body PROGMEM = setHtmlBody(body, h_script);
  // String h_end PROGMEM = getHtmlEnd();

  // mybigString.concat(h_start);
  // mybigString.concat(h_body);
  // mybigString.concat(h_end);

  request->send(200, "text/html", mybigString.c_str());
  mybigString = "";
}

void PrioWebServer::handleAddStream(AsyncWebServerRequest *request, uint8_t *data)
{
  urlManager.addStream(data);
}

void PrioWebServer::handleInputStream(AsyncWebServerRequest *request)
{
  String body PROGMEM = R"(<div id="content-container" class="content"></div>)";
  String mybigString = "";

  String h_start PROGMEM = getHtmlStart();
  String h_script PROGMEM = getAddScript(this->ip);
  String h_body PROGMEM = setHtmlBody(body, h_script);
  String h_end PROGMEM = getHtmlEnd();

  mybigString.concat(h_start);
  mybigString.concat(h_body);
  mybigString.concat(h_end);

  request->send(200, "text/html", mybigString.c_str());
  mybigString = "";
}

void PrioWebServer::handleSettings(AsyncWebServerRequest *request, uint8_t *data)
{ //}, size_t len) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data); //, len);
  if (error)
  {
    request->send(400, "text/plain", "Invalid JSON");
    return;
  }
  serializeJson(doc, Serial);
  urlManager.saveStreams(data); //, len);
}


void PrioWebServer::handleSynctime(AsyncWebServerRequest *request)
{
  Serial.println("Sync time requested");
  Serial.println("Sync time requested" + String(pDateTime.getTime()));
  pDateTime.syncTime(); 
  request->send(200, "text/plain", "ok");
}

String PrioWebServer::createHtmlPage(String body)
{

  String page PROGMEM = "";
  page += getHtmlStart();
  page += setHtmlBody(body, getMainScript(this->ip));
  page += getHtmlEnd();
  return page;
}

String PrioWebServer::getHtmlStart()
{
  String html PROGMEM = R"(
  <!DOCTYPE html>
  <html lang='nl'>
  <head>
  <meta charset='UTF-8'>
  <meta http-equiv='X-UA-Compatible' content='IE=edge'>
  <meta name='viewport' content='width=device-width,initial-scale=1.0'>
  <title>Senior Webradio</title>
  )";
  html += getStyling();
  html += "</head>";
  return html;
}

String PrioWebServer::getTopMenu()
{

  String html PROGMEM = R"(
  <div class='top-menu'>
    <a href='/'>Overzicht</a>
    <a href='/inpustream'>Voeg toe</a>
    <a href='/instellingen'>Instellingen</a>
    <a href='#'>Contact</a>
  </div>
  )";

  return html;
}

String PrioWebServer::setHtmlBody(String body, String script = "")
{
  String html PROGMEM = "";
  html += "<body>";
  html += getTopMenu();
  html += body;
  html += script;
  html += "</body>";
  return html;
}

String PrioWebServer::getHtmlEnd()
{
  String endHtml PROGMEM = "";
  endHtml += "</html>";
  return endHtml;
}