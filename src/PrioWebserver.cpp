#include "PrioWebServer.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

static const char *WEBSERVER_ENABLED_KEY = "websrv_on";
static const char *OTA_BASE_URLS[] = {
  "http://ota.prio-ict.local/webradio",
  "https://ota.prio-ict.nl/webradio"
};
static const size_t OTA_BASE_URL_COUNT = sizeof(OTA_BASE_URLS) / sizeof(OTA_BASE_URLS[0]);

void refreshAlarmDisplayState(bool sendToDisplay);
extern void enterFirmwareUpdateMode();
extern void leaveFirmwareUpdateMode();

PrioWebServer::PrioWebServer(UrlManager &urlManager, AlarmManager &alarmManager, MyPreferences &preferences, int port)
  : urlManager(urlManager), alarmManager(alarmManager), preferences(preferences), server(port)
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
  server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleRoot(request); });

  /* instellingen page , which handles instellingen */
  server.on("/instellingen", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleInstellingen(request); });

  /* import / export page */
  server.on("/importexport", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleImportExportPage(request); });

  /* contact / ota page */
  server.on("/contact", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleContactPage(request); });

  /* alarm configuration page */
  server.on("/alarmen", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleAlarmPage(request); });

  /* deliver the streams in json format */
  server.on("/api/streams", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleApi(request); });

  server.on("/api/deletestream", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleDeleteStream(request); });

  server.on("/api/synctime", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleSynctime(request); });

  server.on("/api/alarms", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleApiAlarms(request); });

  server.on("/api/alarmstatus", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleApiAlarmStatus(request); });

  server.on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleApiSettings(request); });

  server.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleApiOtaStatus(request); });

  server.on("/api/config/export", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleApiExportConfig(request); });

  /* serves the html page to add a stream */
  server.on("/inpustream", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleInputStream(request); });

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

      server.on(
        "/api/alarms",
        HTTP_POST,
        [](AsyncWebServerRequest * request){},
        NULL,
        [this](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
          (void)index;
          (void)total;
          this->handleSaveAlarms(request, data, len);
      });

      server.on(
        "/api/settings",
        HTTP_POST,
        [](AsyncWebServerRequest * request){},
        NULL,
        [this](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
          (void)index;
          (void)total;
          this->handleSaveSettings(request, data, len);
      });

      server.on("/api/ota/check", HTTP_POST, [this](AsyncWebServerRequest *request){ this->handleApiOtaCheck(request); });

      server.on("/api/ota/update", HTTP_POST, [this](AsyncWebServerRequest *request){ this->handleApiOtaUpdate(request, nullptr, 0); });

      server.on(
        "/api/config/import",
        HTTP_POST,
        [](AsyncWebServerRequest * request){},
        NULL,
        [this](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
          String *body = reinterpret_cast<String *>(request->_tempObject);
          if (index == 0)
          {
            if (body != nullptr)
            {
              delete body;
              body = nullptr;
            }
            body = new String();
            if (body != nullptr)
            {
              body->reserve(total);
            }
            request->_tempObject = body;
          }

          if (body == nullptr)
          {
            request->send(500, "application/json", "{\"ok\":false,\"message\":\"Onvoldoende geheugen voor import\"}");
            return;
          }

          body->concat(reinterpret_cast<const char *>(data), len);
          if ((index + len) < total)
          {
            return;
          }

          this->handleApiImportConfig(request, (uint8_t *)body->c_str(), body->length());
          delete body;
          request->_tempObject = nullptr;
      });

      // Logo upload endpoint
      server.on(
        "/api/uploadlogo",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {
          // Response after upload completes
        },
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
          this->handleUploadLogo(request, filename, index, data, len, final);
        }
      );
      
      // Logo upload replace endpoint - replaces existing logo
      server.on(
        "/api/uploadlogo-replace",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {
          // Response after upload completes
        },
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
          this->handleUploadLogoReplace(request, filename, index, data, len, final);
        }
      );
      
      // Logo refresh endpoint - trigger download from URL
      server.on("/api/refreshlogo", HTTP_POST, [this](AsyncWebServerRequest *request){}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
          this->handleRefreshLogo(request, data, len);
        }
      );
     
  server.begin();
  running = true;

  if (preferences.readValue(WEBSERVER_ENABLED_KEY, 1) == 0)
  {
    Serial.println("Webserver blijft uit (opgeslagen instelling)");
    server.end();
    running = false;
  }
}

void PrioWebServer::setEnabled(bool enabled)
{
  preferences.writeValue(WEBSERVER_ENABLED_KEY, enabled ? 1 : 0);

  if (enabled == running)
  {
    return;
  }

  if (enabled)
  {
    server.begin();
  }
  else
  {
    server.end();
  }
  running = enabled;
  Serial.println(enabled ? "Webserver gestart" : "Webserver gestopt");
}

bool PrioWebServer::isRunning() const
{
  return running;
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
  String body PROGMEM = R"(<div id="content-container" class="content"></div>)";
  String mybigString = "";

  int snoozeButtonIndex = (int)preferences.getUInt("snooze_btn_idx", 10);
  int sleepMinutes = (int)preferences.getUInt("sleep_minutes", SLEEP_DEFAULT_MINUTES);
  int backlightMinPercent = (int)preferences.getUInt("bl_min_pct", BACKLIGHT_MIN_PERCENT_DEFAULT);
  int backlightMaxPercent = (int)preferences.getUInt("bl_max_pct", BACKLIGHT_MAX_PERCENT_DEFAULT);
  if (sleepMinutes < SLEEP_MIN_MINUTES)
  {
    sleepMinutes = SLEEP_DEFAULT_MINUTES;
  }
  if (sleepMinutes > SLEEP_MAX_MINUTES)
  {
    sleepMinutes = SLEEP_MAX_MINUTES;
  }
  if (backlightMinPercent < BACKLIGHT_PERCENT_MIN)
  {
    backlightMinPercent = BACKLIGHT_MIN_PERCENT_DEFAULT;
  }
  if (backlightMinPercent > BACKLIGHT_PERCENT_MAX)
  {
    backlightMinPercent = BACKLIGHT_PERCENT_MAX;
  }
  if (backlightMaxPercent < BACKLIGHT_PERCENT_MIN)
  {
    backlightMaxPercent = BACKLIGHT_MAX_PERCENT_DEFAULT;
  }
  if (backlightMaxPercent > BACKLIGHT_PERCENT_MAX)
  {
    backlightMaxPercent = BACKLIGHT_MAX_PERCENT_DEFAULT;
  }
  if (backlightMinPercent > backlightMaxPercent)
  {
    int tmp = backlightMinPercent;
    backlightMinPercent = backlightMaxPercent;
    backlightMaxPercent = tmp;
  }

  String h_start PROGMEM = getHtmlStart();
  String h_script PROGMEM = getSettingsScript(this->ip, snoozeButtonIndex, sleepMinutes, backlightMinPercent, backlightMaxPercent);
  String h_body PROGMEM = setHtmlBody(body, h_script);
  String h_end PROGMEM = getHtmlEnd();

  mybigString.concat(h_start);
  mybigString.concat(h_body);
  mybigString.concat(h_end);

  request->send(200, "text/html", mybigString.c_str());
  mybigString = "";
}

void PrioWebServer::handleImportExportPage(AsyncWebServerRequest *request)
{
  String body PROGMEM = R"(<div id="content-container" class="content"></div>)";
  String mybigString = "";

  String h_start PROGMEM = getHtmlStart();
  String h_script PROGMEM = getImportExportScript(this->ip);
  String h_body PROGMEM = setHtmlBody(body, h_script);
  String h_end PROGMEM = getHtmlEnd();

  mybigString.concat(h_start);
  mybigString.concat(h_body);
  mybigString.concat(h_end);

  request->send(200, "text/html", mybigString.c_str());
  mybigString = "";
}

void PrioWebServer::handleContactPage(AsyncWebServerRequest *request)
{
  String body PROGMEM = R"(<div id="content-container" class="content"></div>)";
  String mybigString = "";

  String h_start PROGMEM = getHtmlStart();
  String h_script PROGMEM = getContactScript(this->ip);
  String h_body PROGMEM = setHtmlBody(body, h_script);
  String h_end PROGMEM = getHtmlEnd();

  mybigString.concat(h_start);
  mybigString.concat(h_body);
  mybigString.concat(h_end);

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

void PrioWebServer::handleAlarmPage(AsyncWebServerRequest *request)
{
  String body PROGMEM = R"(<div id="content-container" class="content"></div>)";
  String mybigString = "";

  String h_start PROGMEM = getHtmlStart();
  String h_script PROGMEM = getAlarmScript(this->ip);
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

void PrioWebServer::handleApiAlarms(AsyncWebServerRequest *request)
{
  JsonDocument doc;

  JsonArray streamsArray = doc["streams"].to<JsonArray>();
  for (uint32_t i = 0; i < urlManager.streamCount; i++)
  {
    JsonObject stream = streamsArray.add<JsonObject>();
    stream["id"] = i;
    stream["name"] = urlManager.Streams[i].name;
  }

  JsonArray alarmsArray = doc["alarms"].to<JsonArray>();
  alarmManager.appendAlarmsJson(alarmsArray);
  doc["maxAlarmsPerDay"] = AlarmManager::MAX_ALARMS_PER_DAY;

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void PrioWebServer::handleApiAlarmStatus(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  doc["status"] = alarmManager.getDisplayStatusLabel();

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void PrioWebServer::handleSaveAlarms(AsyncWebServerRequest *request, uint8_t *data, size_t len)
{
  String error;
  if (!alarmManager.updateFromJson(data, len, urlManager.streamCount, error))
  {
    request->send(400, "application/json", "{\"ok\":false,\"message\":\"" + error + "\"}");
    return;
  }

  refreshAlarmDisplayState(true);
  request->send(200, "application/json", "{\"ok\":true}");
}

void PrioWebServer::handleApiSettings(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  doc["snoozeButtonIndex"] = preferences.getUInt("snooze_btn_idx", 10);
  doc["sleepMinutes"] = preferences.getUInt("sleep_minutes", SLEEP_DEFAULT_MINUTES);
  doc["backlightMinPercent"] = preferences.getUInt("bl_min_pct", BACKLIGHT_MIN_PERCENT_DEFAULT);
  doc["backlightMaxPercent"] = preferences.getUInt("bl_max_pct", BACKLIGHT_MAX_PERCENT_DEFAULT);
  doc["volume"] = preferences.readValue("volume", DEF_VOLUME);
  doc["streamIndex"] = preferences.getUInt("stream_index", 0);

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void PrioWebServer::handleApiExportConfig(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  doc["format"] = "senioren_radio_backup";
  doc["version"] = 1;
  doc["exportedAtMs"] = millis();

  JsonObject settings = doc["settings"].to<JsonObject>();
  settings["snoozeButtonIndex"] = preferences.getUInt("snooze_btn_idx", 10);
  settings["sleepMinutes"] = preferences.getUInt("sleep_minutes", SLEEP_DEFAULT_MINUTES);
  settings["backlightMinPercent"] = preferences.getUInt("bl_min_pct", BACKLIGHT_MIN_PERCENT_DEFAULT);
  settings["backlightMaxPercent"] = preferences.getUInt("bl_max_pct", BACKLIGHT_MAX_PERCENT_DEFAULT);
  settings["volume"] = preferences.readValue("volume", DEF_VOLUME);
  settings["streamIndex"] = preferences.getUInt("stream_index", 0);

  JsonArray streams = doc["streams"].to<JsonArray>();
  for (uint32_t i = 0; i < urlManager.streamCount; i++)
  {
    JsonObject stream = streams.add<JsonObject>();
    stream["id"] = i;
    stream["name"] = urlManager.Streams[i].name;
    stream["url"] = urlManager.Streams[i].url;
    stream["logo"] = urlManager.Streams[i].logo;
  }

  JsonArray alarms = doc["alarms"].to<JsonArray>();
  alarmManager.appendAlarmsJson(alarms);

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void PrioWebServer::handleApiImportConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data, len);
  if (error)
  {
    request->send(400, "application/json", "{\"ok\":false,\"message\":\"Invalid JSON\"}");
    return;
  }

  if (!doc["streams"].is<JsonArray>())
  {
    request->send(400, "application/json", "{\"ok\":false,\"message\":\"streams array ontbreekt\"}");
    return;
  }

  JsonArray incomingStreams = doc["streams"].as<JsonArray>();
  if (incomingStreams.size() > 40)
  {
    request->send(400, "application/json", "{\"ok\":false,\"message\":\"Maximaal 40 streams toegestaan\"}");
    return;
  }

  uint32_t newStreamCount = 0;
  for (JsonVariant streamVar : incomingStreams)
  {
    if (!streamVar.is<JsonObject>())
    {
      request->send(400, "application/json", "{\"ok\":false,\"message\":\"Ongeldig stream item\"}");
      return;
    }

    JsonObject streamObj = streamVar.as<JsonObject>();
    String name = streamObj["name"].is<const char *>() ? String((const char *)streamObj["name"]) : String("");
    String url = streamObj["url"].is<const char *>() ? String((const char *)streamObj["url"]) : String("");
    String logo = streamObj["logo"].is<const char *>() ? String((const char *)streamObj["logo"]) : String("");

    if (name.length() == 0 || url.length() == 0)
    {
      request->send(400, "application/json", "{\"ok\":false,\"message\":\"Elke stream moet naam en url hebben\"}");
      return;
    }

    urlManager.Streams[newStreamCount].id = newStreamCount;
    urlManager.Streams[newStreamCount].name = name;
    urlManager.Streams[newStreamCount].url = url;
    urlManager.Streams[newStreamCount].logo = logo;
    newStreamCount++;
  }

  for (uint32_t i = newStreamCount; i < 40; i++)
  {
    UrlManager::StreamItem emptyItem;
    urlManager.Streams[i] = emptyItem;
  }

  urlManager.streamCount = newStreamCount;
  urlManager.saveToPreferences();

  JsonDocument alarmPayload;
  JsonArray alarmArray = alarmPayload["alarms"].to<JsonArray>();
  if (doc["alarms"].is<JsonArray>())
  {
    for (JsonVariant alarmVar : doc["alarms"].as<JsonArray>())
    {
      alarmArray.add(alarmVar);
    }
  }

  String alarmBuffer;
  serializeJson(alarmPayload, alarmBuffer);
  String alarmError;
  if (!alarmManager.updateFromJson((uint8_t *)alarmBuffer.c_str(), alarmBuffer.length(), urlManager.streamCount, alarmError))
  {
    String errorResponse = String("{\"ok\":false,\"message\":\"Alarm import mislukt: ") + alarmError + "\"}";
    request->send(400, "application/json", errorResponse);
    return;
  }

  if (doc["settings"].is<JsonObject>())
  {
    JsonObject settings = doc["settings"].as<JsonObject>();

    int snoozeButtonIndex = settings["snoozeButtonIndex"].is<int>() ? settings["snoozeButtonIndex"].as<int>() : 10;
    if (snoozeButtonIndex < 0) snoozeButtonIndex = 0;
    if (snoozeButtonIndex > 15) snoozeButtonIndex = 15;
    preferences.putUInt("snooze_btn_idx", (uint32_t)snoozeButtonIndex);
    alarmSnoozeButtonIndex = (uint8_t)snoozeButtonIndex;

    int sleepMinutes = settings["sleepMinutes"].is<int>() ? settings["sleepMinutes"].as<int>() : SLEEP_DEFAULT_MINUTES;
    if (sleepMinutes < SLEEP_MIN_MINUTES) sleepMinutes = SLEEP_DEFAULT_MINUTES;
    if (sleepMinutes > SLEEP_MAX_MINUTES) sleepMinutes = SLEEP_MAX_MINUTES;
    preferences.putUInt("sleep_minutes", (uint32_t)sleepMinutes);

    int backlightMinPercent = settings["backlightMinPercent"].is<int>() ? settings["backlightMinPercent"].as<int>() : BACKLIGHT_MIN_PERCENT_DEFAULT;
    int backlightMaxPercent = settings["backlightMaxPercent"].is<int>() ? settings["backlightMaxPercent"].as<int>() : BACKLIGHT_MAX_PERCENT_DEFAULT;
    if (backlightMinPercent < BACKLIGHT_PERCENT_MIN) backlightMinPercent = BACKLIGHT_MIN_PERCENT_DEFAULT;
    if (backlightMinPercent > BACKLIGHT_PERCENT_MAX) backlightMinPercent = BACKLIGHT_PERCENT_MAX;
    if (backlightMaxPercent < BACKLIGHT_PERCENT_MIN) backlightMaxPercent = BACKLIGHT_MAX_PERCENT_DEFAULT;
    if (backlightMaxPercent > BACKLIGHT_PERCENT_MAX) backlightMaxPercent = BACKLIGHT_MAX_PERCENT_DEFAULT;
    if (backlightMinPercent > backlightMaxPercent)
    {
      int tmp = backlightMinPercent;
      backlightMinPercent = backlightMaxPercent;
      backlightMaxPercent = tmp;
    }
    preferences.putUInt("bl_min_pct", (uint32_t)backlightMinPercent);
    preferences.putUInt("bl_max_pct", (uint32_t)backlightMaxPercent);
    gBacklightMinPercent = (uint8_t)backlightMinPercent;
    gBacklightMaxPercent = (uint8_t)backlightMaxPercent;

    int volume = settings["volume"].is<int>() ? settings["volume"].as<int>() : DEF_VOLUME;
    if (volume < MIN_VOLUME) volume = MIN_VOLUME;
    if (volume > MAX_VOLUME) volume = MAX_VOLUME;
    preferences.writeValue("volume", volume);

    int streamIndex = settings["streamIndex"].is<int>() ? settings["streamIndex"].as<int>() : 0;
    if (streamIndex < 0)
    {
      streamIndex = 0;
    }
    if (urlManager.streamCount == 0)
    {
      streamIndex = 0;
    }
    else if (streamIndex >= (int)urlManager.streamCount)
    {
      streamIndex = (int)urlManager.streamCount - 1;
    }
    preferences.putUInt("stream_index", (uint32_t)streamIndex);
  }

  refreshAlarmDisplayState(true);
  request->send(200, "application/json", "{\"ok\":true}");
}

void PrioWebServer::handleSaveSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data, len);
  if (error)
  {
    request->send(400, "application/json", "{\"ok\":false,\"message\":\"Invalid JSON\"}");
    return;
  }

  int snoozeButtonIndex = doc["snoozeButtonIndex"].is<int>() ? doc["snoozeButtonIndex"].as<int>() : 10;
  if (snoozeButtonIndex < 0 || snoozeButtonIndex > 15)
  {
    request->send(400, "application/json", "{\"ok\":false,\"message\":\"snoozeButtonIndex moet 0..15 zijn\"}");
    return;
  }

  int sleepMinutes = doc["sleepMinutes"].is<int>() ? doc["sleepMinutes"].as<int>() : SLEEP_DEFAULT_MINUTES;
  if (sleepMinutes < SLEEP_MIN_MINUTES || sleepMinutes > SLEEP_MAX_MINUTES)
  {
    request->send(400, "application/json", "{\"ok\":false,\"message\":\"sleepMinutes moet 1..180 zijn\"}");
    return;
  }

  int backlightMinPercent = doc["backlightMinPercent"].is<int>() ? doc["backlightMinPercent"].as<int>() : BACKLIGHT_MIN_PERCENT_DEFAULT;
  if (backlightMinPercent < BACKLIGHT_PERCENT_MIN || backlightMinPercent > BACKLIGHT_PERCENT_MAX)
  {
    request->send(400, "application/json", "{\"ok\":false,\"message\":\"backlightMinPercent moet 0..100 zijn\"}");
    return;
  }

  int backlightMaxPercent = doc["backlightMaxPercent"].is<int>() ? doc["backlightMaxPercent"].as<int>() : BACKLIGHT_MAX_PERCENT_DEFAULT;
  if (backlightMaxPercent < BACKLIGHT_PERCENT_MIN || backlightMaxPercent > BACKLIGHT_PERCENT_MAX)
  {
    request->send(400, "application/json", "{\"ok\":false,\"message\":\"backlightMaxPercent moet 0..100 zijn\"}");
    return;
  }

  if (backlightMinPercent > backlightMaxPercent)
  {
    request->send(400, "application/json", "{\"ok\":false,\"message\":\"backlightMinPercent mag niet groter zijn dan backlightMaxPercent\"}");
    return;
  }

  preferences.putUInt("snooze_btn_idx", (uint32_t)snoozeButtonIndex);
  preferences.putUInt("sleep_minutes", (uint32_t)sleepMinutes);
  preferences.putUInt("bl_min_pct", (uint32_t)backlightMinPercent);
  preferences.putUInt("bl_max_pct", (uint32_t)backlightMaxPercent);
  gBacklightMinPercent = (uint8_t)backlightMinPercent;
  gBacklightMaxPercent = (uint8_t)backlightMaxPercent;
  alarmSnoozeButtonIndex = (uint8_t)snoozeButtonIndex;
  request->send(200, "application/json", "{\"ok\":true}");
}

void PrioWebServer::handleUploadLogo(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  static File uploadFile;
  static String uploadPath;
  
  // First chunk - open file
  if (index == 0) {
    Serial.printf("[UPLOAD] Start: %s\n", filename.c_str());
    
    // Create /StreamLogos directory if it doesn't exist
    if (!LittleFS.exists("/StreamLogos")) {
      if (!LittleFS.mkdir("/StreamLogos")) {
        Serial.println("[UPLOAD] Failed to create /StreamLogos directory");
        request->send(500, "application/json", "{\"error\":\"Failed to create directory\"}");
        return;
      }
    }
    
    // Build full path
    uploadPath = "/StreamLogos/" + filename;
    
    // Check if file already exists and delete it
    if (LittleFS.exists(uploadPath)) {
      LittleFS.remove(uploadPath);
      Serial.printf("[UPLOAD] Removed existing file: %s\n", uploadPath.c_str());
    }
    
    // Open file for writing
    uploadFile = LittleFS.open(uploadPath, "w");
    if (!uploadFile) {
      Serial.println("[UPLOAD] Failed to open file for writing");
      request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
      return;
    }
    
    Serial.printf("[UPLOAD] Free heap: %d bytes\n", ESP.getFreeHeap());
  }
  
  // Write chunk
  if (uploadFile && len) {
    size_t written = uploadFile.write(data, len);
    if (written != len) {
      Serial.printf("[UPLOAD] Write failed: expected %d, wrote %d\n", len, written);
    }
  }
  
  // Last chunk - close file and send response
  if (final) {
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("[UPLOAD] Complete: %s (%d bytes)\n", uploadPath.c_str(), index + len);
      
      // Send success response with file path
      String response = "{\"ok\":true,\"path\":\"" + uploadPath + "\",\"size\":" + String(index + len) + "}";
      request->send(200, "application/json", response);
    } else {
      Serial.println("[UPLOAD] Failed - file not open");
      request->send(500, "application/json", "{\"error\":\"Upload failed\"}");
    }
  }
}

void PrioWebServer::handleUploadLogoReplace(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  static File uploadFile;
  static String uploadPath;
  static String oldLogoPath;
  
  // First chunk - open file and handle old logo deletion
  if (index == 0) {
    Serial.printf("[UPLOAD-REPLACE] Start: %s\n", filename.c_str());
    
    // Get oldLogoPath parameter from request
    if (request->hasParam("oldLogoPath", true)) {
      oldLogoPath = request->getParam("oldLogoPath", true)->value();
      Serial.printf("[UPLOAD-REPLACE] Old logo path: %s\n", oldLogoPath.c_str());
      
      // Delete old logo file if it exists
      if (oldLogoPath.length() > 0 && LittleFS.exists(oldLogoPath)) {
        if (LittleFS.remove(oldLogoPath)) {
          Serial.printf("[UPLOAD-REPLACE] Removed old logo: %s\n", oldLogoPath.c_str());
        } else {
          Serial.printf("[UPLOAD-REPLACE] Failed to remove old logo: %s\n", oldLogoPath.c_str());
        }
      }
    }
    
    // Create /StreamLogos directory if it doesn't exist
    if (!LittleFS.exists("/StreamLogos")) {
      if (!LittleFS.mkdir("/StreamLogos")) {
        Serial.println("[UPLOAD-REPLACE] Failed to create /StreamLogos directory");
        request->send(500, "application/json", "{\"error\":\"Failed to create directory\"}");
        return;
      }
    }
    
    // Build full path
    uploadPath = "/StreamLogos/" + filename;
    
    // Check if file already exists and delete it
    if (LittleFS.exists(uploadPath)) {
      LittleFS.remove(uploadPath);
      Serial.printf("[UPLOAD-REPLACE] Removed existing file: %s\n", uploadPath.c_str());
    }
    
    // Open file for writing
    uploadFile = LittleFS.open(uploadPath, "w");
    if (!uploadFile) {
      Serial.println("[UPLOAD-REPLACE] Failed to open file for writing");
      request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
      return;
    }
    
    Serial.printf("[UPLOAD-REPLACE] Free heap: %d bytes\n", ESP.getFreeHeap());
  }
  
  // Write chunk
  if (uploadFile && len) {
    size_t written = uploadFile.write(data, len);
    if (written != len) {
      Serial.printf("[UPLOAD-REPLACE] Write failed: expected %d, wrote %d\n", len, written);
    }
  }
  
  // Last chunk - close file and send response
  if (final) {
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("[UPLOAD-REPLACE] Complete: %s (%d bytes)\n", uploadPath.c_str(), index + len);
      
      // Send success response with file path
      String response = "{\"ok\":true,\"path\":\"" + uploadPath + "\",\"size\":" + String(index + len) + "}";
      request->send(200, "application/json", response);
    } else {
      Serial.println("[UPLOAD-REPLACE] Failed - file not open");
      request->send(500, "application/json", "{\"error\":\"Upload failed\"}");
    }
  }
}

void PrioWebServer::handleRefreshLogo(AsyncWebServerRequest *request, uint8_t *data, size_t len)
{
  // DISABLED: Logo download conflicts with Audio library SSL
  Serial.println("[REFRESH] Logo download is disabled - use upload instead");
  String response = "{\"ok\":false,\"error\":\"Logo download is uitgeschakeld. Gebruik de upload functie.\"}";
  request->send(503, "application/json", response);
}

OtaStatusSnapshot PrioWebServer::getOtaStatusSnapshot()
{
  OtaStatusSnapshot status;
  status.currentVersion = FIRMWARE_VERSION;
  status.latestVersion = otaLatestVersion;
  status.sourceBaseUrl = otaSourceBaseUrl;
  status.lastError = otaLastError;
  status.updateMessage = otaLastUpdateMessage;
  status.updateAvailable = otaUpdateAvailable;
  status.checkInProgress = otaCheckInProgress;
  status.updateInProgress = otaUpdateInProgress;
  status.uptimeSeconds = millis() / 1000UL;
  status.lastCheckAgeSeconds = otaLastCheckMs == 0 ? 0 : (uint32_t)((millis() - otaLastCheckMs) / 1000UL);
  return status;
}

void PrioWebServer::otaCheckTaskEntry(void *param)
{
  PrioWebServer *self = static_cast<PrioWebServer *>(param);
  if (self != nullptr)
  {
    self->otaCheckTaskBody();
  }
  vTaskDelete(nullptr);
}

void PrioWebServer::otaCheckTaskBody()
{
  checkOtaVersion();
  otaCheckInProgress = false;
}

void PrioWebServer::otaUpdateTaskEntry(void *param)
{
  PrioWebServer *self = static_cast<PrioWebServer *>(param);
  if (self != nullptr)
  {
    self->otaUpdateTaskBody();
  }
  vTaskDelete(nullptr);
}

void PrioWebServer::otaUpdateTaskBody()
{
  enterFirmwareUpdateMode();

  String message;
  bool ok = runOtaUpdate(message);
  otaLastUpdateMessage = message;
  if (!ok && message.length() == 0)
  {
    otaLastUpdateMessage = "OTA update mislukt";
  }

  // Bij een echte succesvolle flash volgt doorgaans een reboot; bij falen of
  // "geen update" gaan we terug naar normale bedrijfsmodus.
  if (!ok || !otaLastUpdateMessage.startsWith("OTA update succesvol"))
  {
    leaveFirmwareUpdateMode();
  }

  otaUpdateInProgress = false;
}

bool PrioWebServer::startOtaCheckAsync()
{
  if (otaUpdateInProgress)
  {
    otaLastError = "OTA update is bezig";
    return false;
  }

  if (otaCheckInProgress)
  {
    return true;
  }

  otaCheckInProgress = true;
  BaseType_t taskOk = xTaskCreatePinnedToCore(
      PrioWebServer::otaCheckTaskEntry,
      "ota_check",
      12288,
      this,
      1,
      nullptr,
      1);

  if (taskOk != pdPASS)
  {
    otaCheckInProgress = false;
    otaLastError = "Kon OTA checktask niet starten";
    return false;
  }

  return true;
}

bool PrioWebServer::startOtaUpdateAsync(String &message)
{
  if (otaUpdateInProgress)
  {
    message = "OTA update is al bezig";
    return true;
  }

  if (otaCheckInProgress)
  {
    message = "Wacht tot OTA versiecontrole klaar is";
    return false;
  }

  otaUpdateInProgress = true;
  otaLastUpdateMessage = "OTA update gestart";

  BaseType_t taskOk = xTaskCreatePinnedToCore(
      PrioWebServer::otaUpdateTaskEntry,
      "ota_update",
      16384,
      this,
      1,
      nullptr,
      1);

  if (taskOk != pdPASS)
  {
    otaUpdateInProgress = false;
    otaLastUpdateMessage = "Kon OTA updatetaak niet starten";
    message = otaLastUpdateMessage;
    return false;
  }

  message = "OTA update gestart";
  return true;
}

bool PrioWebServer::checkOtaVersion()
{
  otaLastError = "";

  for (size_t i = 0; i < OTA_BASE_URL_COUNT; i++)
  {
    const String baseUrl = OTA_BASE_URLS[i];
    const String manifestUrl = baseUrl + "/version.json";
    int statusCode = -1;
    String payload;

    if (manifestUrl.startsWith("https://"))
    {
      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient http;
      http.setTimeout(4000);
      if (!http.begin(client, manifestUrl))
      {
        otaLastError = "Kan OTA manifest niet openen: " + manifestUrl;
        continue;
      }
      statusCode = http.GET();
      if (statusCode > 0)
      {
        payload = http.getString();
      }
      http.end();
    }
    else
    {
      WiFiClient client;
      HTTPClient http;
      http.setTimeout(4000);
      if (!http.begin(client, manifestUrl))
      {
        otaLastError = "Kan OTA manifest niet openen: " + manifestUrl;
        continue;
      }
      statusCode = http.GET();
      if (statusCode > 0)
      {
        payload = http.getString();
      }
      http.end();
    }

    delay(1);

    if (statusCode != 200)
    {
      otaLastError = "OTA manifest HTTP status " + String(statusCode) + " op " + manifestUrl;
      continue;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
    {
      otaLastError = "OTA manifest JSON ongeldig op " + manifestUrl;
      continue;
    }

    String latestVersion = doc["version"].is<const char *>() ? String((const char *)doc["version"]) : String("");
    if (latestVersion.length() == 0)
    {
      otaLastError = "OTA manifest mist veld 'version' op " + manifestUrl;
      continue;
    }

    otaLatestVersion = latestVersion;
    otaSourceBaseUrl = baseUrl;
    otaUpdateAvailable = otaLatestVersion != String(FIRMWARE_VERSION);
    otaLastCheckMs = millis();
    otaLastError = "";
    return true;
  }

  otaLastCheckMs = millis();
  otaUpdateAvailable = false;
  return false;
}

bool PrioWebServer::runOtaUpdate(String &message)
{
  bool foundManifest = checkOtaVersion();
  String selectedBaseUrl = otaSourceBaseUrl;

  if (!foundManifest)
  {
    for (size_t i = 0; i < OTA_BASE_URL_COUNT; i++)
    {
      selectedBaseUrl = OTA_BASE_URLS[i];
      if (selectedBaseUrl.length() > 0)
      {
        break;
      }
    }
  }

  if (selectedBaseUrl.length() == 0)
  {
    message = "Geen OTA URL beschikbaar";
    return false;
  }

  String firmwareUrl = selectedBaseUrl + "/firmware.bin";
  Serial.println("[OTA] Update gestart vanaf: " + firmwareUrl);

  t_httpUpdate_return result;
  if (firmwareUrl.startsWith("https://"))
  {
    WiFiClientSecure client;
    client.setInsecure();
    result = httpUpdate.update(client, firmwareUrl);
  }
  else
  {
    WiFiClient client;
    result = httpUpdate.update(client, firmwareUrl);
  }

  switch (result)
  {
  case HTTP_UPDATE_FAILED:
    message = "OTA mislukt: " + httpUpdate.getLastErrorString();
    otaLastError = message;
    return false;
  case HTTP_UPDATE_NO_UPDATES:
    message = "Server meldt geen update (firmware is mogelijk gelijk).";
    otaLastError = "";
    return true;
  case HTTP_UPDATE_OK:
    message = "OTA update succesvol. Apparaat herstart nu.";
    otaLastError = "";
    return true;
  default:
    message = "Onbekende OTA status";
    otaLastError = message;
    return false;
  }
}

void PrioWebServer::handleApiOtaStatus(AsyncWebServerRequest *request)
{
  OtaStatusSnapshot status = getOtaStatusSnapshot();

  JsonDocument doc;
  doc["currentVersion"] = status.currentVersion;
  doc["latestVersion"] = status.latestVersion;
  doc["sourceBaseUrl"] = status.sourceBaseUrl;
  doc["lastError"] = status.lastError;
  doc["updateMessage"] = status.updateMessage;
  doc["updateAvailable"] = status.updateAvailable;
  doc["checkInProgress"] = status.checkInProgress;
  doc["updateInProgress"] = status.updateInProgress;
  doc["uptimeSeconds"] = status.uptimeSeconds;
  doc["lastCheckAgeSeconds"] = status.lastCheckAgeSeconds;

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void PrioWebServer::handleApiOtaCheck(AsyncWebServerRequest *request)
{
  bool ok = startOtaCheckAsync();
  OtaStatusSnapshot status = getOtaStatusSnapshot();

  JsonDocument doc;
  doc["ok"] = ok;
  doc["message"] = ok ? "OTA controle gestart" : (status.lastError.length() > 0 ? status.lastError : "OTA controle kon niet starten");
  doc["inProgress"] = status.checkInProgress;

  JsonObject st = doc["status"].to<JsonObject>();
  st["currentVersion"] = status.currentVersion;
  st["latestVersion"] = status.latestVersion;
  st["sourceBaseUrl"] = status.sourceBaseUrl;
  st["lastError"] = status.lastError;
  st["updateMessage"] = status.updateMessage;
  st["updateAvailable"] = status.updateAvailable;
  st["checkInProgress"] = status.checkInProgress;
  st["updateInProgress"] = status.updateInProgress;
  st["uptimeSeconds"] = status.uptimeSeconds;
  st["lastCheckAgeSeconds"] = status.lastCheckAgeSeconds;

  String response;
  serializeJson(doc, response);
  request->send(ok ? 202 : 503, "application/json", response);
}

void PrioWebServer::handleApiOtaUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len)
{
  (void)data;
  (void)len;

  String message;
  bool ok = startOtaUpdateAsync(message);

  JsonDocument doc;
  doc["ok"] = ok;
  doc["message"] = message;
  doc["inProgress"] = otaUpdateInProgress;
  doc["currentVersion"] = String(FIRMWARE_VERSION);
  doc["latestVersion"] = otaLatestVersion;
  doc["sourceBaseUrl"] = otaSourceBaseUrl;

  String response;
  serializeJson(doc, response);
  request->send(ok ? 202 : 500, "application/json", response);
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
  <div class='top-menu' id='topMenu'>
    <button class='hamburger' id='hamburgerBtn' aria-label='Menu'>&#9776;</button>
    <a href='/'>Overzicht</a>
    <a href='/inpustream'>Voeg toe</a>
    <a href='/alarmen'>Alarmen</a>
    <a href='/instellingen'>Instellingen</a>
    <a href='/importexport'>Import/Export</a>
    <a href='/contact'>Contact</a>
    <span class='alarm-badge alarm-uit' id='alarm-status-badge'>Alarm: uit</span>
  </div>
  <script>
    // Hamburger menu toggle voor mobiel
    document.getElementById('hamburgerBtn').addEventListener('click', function() {
      document.getElementById('topMenu').classList.toggle('mobile-active');
    });

    (function () {
      const badge = document.getElementById("alarm-status-badge");

      function setBadge(status) {
        const normalized = (status || "uit").toLowerCase();
        badge.classList.remove("alarm-actief", "alarm-snooze", "alarm-ingesteld", "alarm-uit");

        if (normalized === "alarm actief" || normalized === "actief") {
          badge.classList.add("alarm-actief");
          badge.textContent = "Alarm: actief";
          return;
        }

        if (normalized === "snooze wacht" || normalized === "snooze") {
          badge.classList.add("alarm-snooze");
          badge.textContent = "Alarm: snooze";
          return;
        }

        if (normalized === "ingesteld") {
          badge.classList.add("alarm-ingesteld");
          badge.textContent = "Alarm: ingesteld";
          return;
        }

        badge.classList.add("alarm-uit");
        badge.textContent = "Alarm: uit";
      }

      async function refreshAlarmStatus() {
        try {
          const response = await fetch("/api/alarmstatus", { method: "GET" });
          if (!response.ok) {
            return;
          }
          const data = await response.json();
          setBadge(data.status);
        } catch (e) {
          // keep previous badge status when request fails
        }
      }

      window.refreshAlarmStatusBadge = refreshAlarmStatus;
      refreshAlarmStatus();
      setInterval(refreshAlarmStatus, 5000);
    })();
  </script>
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