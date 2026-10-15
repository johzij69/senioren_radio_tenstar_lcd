#include "PrioWebServer.h"
#include "NetworkSettings.h"
#include "AlarmAudio.h"
#include <new>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

static const char *WEBSERVER_ENABLED_KEY = "websrv_on";
static const char *OTA_BASE_URLS[] = {
  "https://ota.prio-ict.nl/webradio",
  "http://ota.prio-ict.local/webradio"
};
static const size_t OTA_BASE_URL_COUNT = sizeof(OTA_BASE_URLS) / sizeof(OTA_BASE_URLS[0]);

struct AlarmAudioUpload
{
  File file;
  size_t received = 0;
  size_t expected = 0;
  bool owner = false;
  bool complete = false;
  int status = 200;
  String error;
};

static size_t alarmAudioUploadLimit()
{
  size_t total = LittleFS.totalBytes();
  size_t used = LittleFS.usedBytes();
  size_t freeBytes = total > used ? total - used : 0;
  if (freeBytes <= AlarmAudio::RESERVED_BYTES) return 0;
  size_t available = freeBytes - AlarmAudio::RESERVED_BYTES;
  return available < AlarmAudio::MAX_BYTES ? available : AlarmAudio::MAX_BYTES;
}

void refreshAlarmDisplayState(bool sendToDisplay);
extern bool enterFirmwareUpdateMode();
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

  server.on("/netwerk", HTTP_GET, [this](AsyncWebServerRequest *request){ this->handleNetworkPage(request); });

  server.on("/api/network", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json;
    if (!NetworkSettings::getStatus(json))
    {
      request->send(503, "application/json", "{\"message\":\"Netwerkstatus is bezig\"}");
      return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  server.on("/api/network/scan", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool accepted = NetworkSettings::requestScan();
    request->send(accepted ? 202 : 409, "application/json",
      accepted ? "{\"message\":\"Wifi-scan aangevraagd\"}" : "{\"message\":\"Netwerk is bezig; probeer opnieuw\"}");
  });

  server.on("/api/network/select", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("bssid", true) || !request->hasParam("timeout", true))
    {
      request->send(400, "application/json", "{\"message\":\"BSSID en timeout ontbreken\"}");
      return;
    }
    String timeout = request->getParam("timeout", true)->value();
    bool validTimeout = timeout.length() > 0 && timeout.length() <= 3;
    for (size_t index = 0; index < timeout.length(); ++index)
    {
      if (!isDigit(timeout[index])) validTimeout = false;
    }
    if (!validTimeout || timeout.toInt() < 5 || timeout.toInt() > 300)
    {
      request->send(400, "application/json", "{\"message\":\"Timeout moet 5 tot 300 seconden zijn\"}");
      return;
    }
    bool accepted = NetworkSettings::selectAccessPoint(request->getParam("bssid", true)->value(), timeout.toInt());
    request->send(accepted ? 202 : 409, "application/json", accepted ?
      "{\"message\":\"Keuze ontvangen; netwerkvoorkeur wordt opgeslagen\"}" :
      "{\"message\":\"Selecteer een gescand accesspoint van het huidige netwerk, of probeer opnieuw\"}");
  });

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

  server.on("/api/alarm-audio", HTTP_GET, [this](AsyncWebServerRequest *request) {
    handleAlarmAudioStatus(request);
  });
  server.on("/api/alarm-audio", HTTP_POST,
    [this](AsyncWebServerRequest *request) { finishAlarmAudioUpload(request); }, nullptr,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      handleAlarmAudioUpload(request, data, len, index, total);
    });

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

void PrioWebServer::handleAlarmAudioStatus(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  File file = LittleFS.open(AlarmAudio::PATH, "r");
  doc["exists"] = static_cast<bool>(file);
  doc["size"] = file ? file.size() : 0;
  doc["maxBytes"] = AlarmAudio::MAX_BYTES;
  doc["uploadLimitBytes"] = alarmAudioUploadLimit();
  doc["busy"] = alarmAudioUploadBusy.load() || otaUpdateInProgress.load() ||
    alarmManager.getRuntimeStatus() != AlarmManager::STATUS_OFF;
  doc["format"] = "mp3";
  String json;
  serializeJson(doc, json);
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

void PrioWebServer::clearAlarmAudioUpload(AsyncWebServerRequest *request)
{
  auto *upload = static_cast<AlarmAudioUpload *>(request->_tempObject);
  request->_tempObject = nullptr;
  if (!upload) return;
  if (upload->owner)
  {
    upload->file.close();
    LittleFS.remove(AlarmAudio::TEMP_PATH);
    alarmAudioUploadBusy = false;
  }
  delete upload;
}

void PrioWebServer::handleAlarmAudioUpload(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  if (index == 0)
  {
    auto *upload = new (std::nothrow) AlarmAudioUpload();
    if (!upload) return;
    request->_tempObject = upload;
    request->onDisconnect([this, request]() { clearAlarmAudioUpload(request); });
    bool expected = false;
    upload->owner = alarmAudioUploadBusy.compare_exchange_strong(expected, true);
    upload->expected = total;
    String name = request->hasParam("name") ? request->getParam("name")->value() : String("");
    name.toLowerCase();
    if (!upload->owner || otaUpdateInProgress || gFirmwareUpdateInProgress ||
        alarmManager.getRuntimeStatus() != AlarmManager::STATUS_OFF)
    {
      upload->status = 409;
      upload->error = "Upload niet mogelijk tijdens een alarm, firmware-update of andere upload";
    }
    else if (!name.endsWith(".mp3"))
    {
      upload->status = 415;
      upload->error = "Alleen MP3-bestanden zijn toegestaan";
    }
    else if (total == 0 || total > AlarmAudio::MAX_BYTES || request->contentLength() != total)
    {
      upload->status = 413;
      upload->error = "MP3 moet 1 tot 524288 bytes groot zijn";
    }
    else
    {
      LittleFS.remove(AlarmAudio::TEMP_PATH);
      if (total > alarmAudioUploadLimit())
      {
        upload->status = 507;
        upload->error = "Onvoldoende vrije ruimte; het bestaande alarmbestand blijft behouden";
      }
      else
      {
        upload->file = LittleFS.open(AlarmAudio::TEMP_PATH, "w");
        if (!upload->file)
        {
          upload->status = 500;
          upload->error = "Tijdelijk alarmbestand kon niet worden geopend";
        }
      }
    }
  }
  auto *upload = static_cast<AlarmAudioUpload *>(request->_tempObject);
  if (!upload || upload->status != 200) return;
  if (index != upload->received || len > AlarmAudio::MAX_BYTES - upload->received ||
      len > upload->expected - upload->received)
  {
    upload->status = 413;
    upload->error = "Upload overschrijdt de toegestane grootte of is onvolledig";
    return;
  }
  if (upload->file.write(data, len) != len)
  {
    upload->status = 507;
    upload->error = "Schrijven mislukt; het bestaande alarmbestand blijft behouden";
    return;
  }
  upload->received += len;
  if (upload->received == upload->expected)
  {
    upload->file.close();
    File candidate = LittleFS.open(AlarmAudio::TEMP_PATH, "r");
    if (!AlarmAudio::hasMp3Header(candidate))
    {
      upload->status = 415;
      upload->error = "Geen herkenbaar MP3-bestand met volledige MPEG Layer III-frames";
      return;
    }
    upload->complete = true;
  }
}

void PrioWebServer::finishAlarmAudioUpload(AsyncWebServerRequest *request)
{
  auto *upload = static_cast<AlarmAudioUpload *>(request->_tempObject);
  int status = upload ? upload->status : 400;
  String message = upload ? upload->error : String("Geen audiobestand ontvangen");
  if (upload && status == 200)
  {
    if (!upload->complete)
    {
      status = 400;
      message = "Upload is onvolledig; het bestaande alarmbestand blijft behouden";
    }
    else if (gFirmwareUpdateInProgress || otaUpdateInProgress || alarmManager.getRuntimeStatus() != AlarmManager::STATUS_OFF)
    {
      status = 409;
      message = "Alarm of update gestart; het bestaande alarmbestand blijft behouden";
    }
    else if (!LittleFS.rename(AlarmAudio::TEMP_PATH, AlarmAudio::PATH))
    {
      status = 500;
      message = "Vervangen van het alarmbestand mislukt";
    }
    else message = "Offline alarmbestand vervangen";
  }
  JsonDocument doc;
  doc["ok"] = status == 200;
  doc["message"] = message;
  String json;
  serializeJson(doc, json);
  clearAlarmAudioUpload(request);
  request->send(status, "application/json", json);
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

void PrioWebServer::processOtaRequests()
{
  if (otaUpdateRequested.exchange(false))
  {
    otaUpdateTaskBody();
  }
  else if (otaCheckRequested.exchange(false))
  {
    otaCheckTaskBody();
  }
}

void PrioWebServer::otaCheckTaskBody()
{
  checkOtaVersion();
  otaCheckInProgress = false;
}

void PrioWebServer::otaUpdateTaskBody()
{
  if (!enterFirmwareUpdateMode())
  {
    otaLastUpdateMessage = "OTA afgebroken: audio-geheugen kon niet veilig worden vrijgegeven";
    otaLastError = otaLastUpdateMessage;
    leaveFirmwareUpdateMode();
    otaUpdateInProgress = false;
    return;
  }

  Serial.printf("[OTA] Voor download: intern vrij=%u, grootste blok=%u, PSRAM vrij=%u\n",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned)ESP.getFreePsram());

  String message;
  bool updated = false;
  bool ok = runOtaUpdate(message, updated);
  otaLastUpdateMessage = message;
  if (!ok && message.length() == 0)
  {
    otaLastUpdateMessage = "OTA update mislukt";
  }

  Serial.println("[OTA] " + otaLastUpdateMessage);
  if (ok && updated)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP.restart();
    return;
  }

  leaveFirmwareUpdateMode();
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
  otaCheckRequested = true;
  return true;
}

bool PrioWebServer::startOtaUpdateAsync(String &message)
{
  if (alarmAudioUploadBusy)
  {
    message = "Wacht tot de upload van het alarmbestand klaar is";
    return false;
  }
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

  otaUpdateRequested = true;
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

bool PrioWebServer::runOtaUpdate(String &message, bool &updated)
{
  updated = false;
  if (WiFi.status() != WL_CONNECTED)
  {
    message = "WiFi niet verbonden; OTA update afgebroken";
    otaLastError = message;
    return false;
  }

  if (!checkOtaVersion())
  {
    message = "OTA manifest niet bereikbaar: " + otaLastError;
    return false;
  }

  String selectedBaseUrl = otaSourceBaseUrl;

  if (selectedBaseUrl.length() == 0)
  {
    message = "Geen geldige OTA bron beschikbaar";
    otaLastError = message;
    return false;
  }

  String firmwareUrl = selectedBaseUrl + "/firmware.bin";
  Serial.println("[OTA] Update gestart vanaf: " + firmwareUrl);

  HTTPUpdate updater;
  updater.rebootOnUpdate(false);
  t_httpUpdate_return result;
  if (firmwareUrl.startsWith("https://"))
  {
    WiFiClientSecure client;
    client.setInsecure();
    result = updater.update(client, firmwareUrl);
  }
  else
  {
    WiFiClient client;
    result = updater.update(client, firmwareUrl);
  }

  switch (result)
  {
  case HTTP_UPDATE_FAILED:
    message = "OTA mislukt: " + updater.getLastErrorString();
    otaLastError = message;
    return false;
  case HTTP_UPDATE_NO_UPDATES:
    message = "Server meldt geen update (firmware is mogelijk gelijk).";
    otaLastError = "";
    return true;
  case HTTP_UPDATE_OK:
    updated = true;
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
  doc["inProgress"] = otaUpdateInProgress.load();
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

void PrioWebServer::handleNetworkPage(AsyncWebServerRequest *request)
{
  String body = R"HTML(
  <style>
    .network-content { max-width: 760px; padding-bottom: 24px; }
    .network-content h1 { font-size: 24px; }
    .network-content dl { display: grid; grid-template-columns: auto 1fr; gap: 8px 16px; }
    .network-content dd { margin: 0; overflow-wrap: anywhere; }
    .network-content fieldset { border: 0; padding: 12px 0; margin: 0; min-width: 0; }
    .network-content table { width: 100%; border-collapse: collapse; font-size: 14px; }
    .network-content th, .network-content td { padding: 10px 6px; text-align: left; border-bottom: 1px solid #ddd; overflow-wrap: anywhere; }
    .network-content caption { text-align: left; margin: 12px 0; font-weight: bold; }
    .network-content .network-table { overflow-x: auto; }
    .network-content table { min-width: 440px; }
    .network-content button { cursor: pointer; min-height: 40px; padding: 8px 14px; margin: 8px 8px 8px 0; }
    .network-content button:disabled { cursor: default; }
    .network-content input[type=number] { width: 85px; min-height: 40px; }
    .network-content input[type=radio] { width: 20px; height: 20px; }
    #network-message { min-height: 36px; overflow-wrap: anywhere; }
  </style>
  <main class="content network-content">
    <h1>Netwerkinstellingen</h1>
    <dl>
      <dt>Netwerk</dt><dd id="network-ssid">-</dd>
      <dt>Verbonden met</dt><dd id="network-current">-</dd>
      <dt>Voorkeursaccesspoint</dt><dd id="network-preferred">Geen voorkeur</dd>
    </dl>
    <form id="network-form">
      <fieldset id="network-controls">
        <label for="network-timeout">Timeout voor terugval (seconden)</label>
        <input id="network-timeout" name="timeout" type="number" min="5" max="300" value="30" required>
        <div>
          <button type="button" id="network-scan">&#8635; Scannen</button>
          <button type="submit" id="network-save" disabled>Opslaan en verbinden</button>
        </div>
        <div class="network-table">
          <table>
            <caption>Accesspoints binnen bereik (2,4 GHz)</caption>
            <thead><tr><th scope="col">Keuze</th><th scope="col">Netwerk / BSSID</th><th scope="col">Signaal</th><th scope="col">Kanaal</th></tr></thead>
            <tbody id="network-list"></tbody>
          </table>
        </div>
      </fieldset>
    </form>
    <p id="network-message" role="status" aria-live="polite">Netwerkstatus ophalen...</p>
  </main>)HTML";
  String script = R"JS(<script>
  (() => {
    const list = document.getElementById('network-list');
    const message = document.getElementById('network-message');
    const scan = document.getElementById('network-scan');
    const save = document.getElementById('network-save');
    const timeout = document.getElementById('network-timeout');
    const controls = document.getElementById('network-controls');
    let initialized = false;
    let busy = false;
    let scanPending = false;
    let scanRequestedAt = 0;
    let selectionPending = false;
    let selectionRequestedAt = 0;
    let lastRows = '';
    async function api(path, options) {
      const response = await fetch(path, {cache: 'no-store', ...options});
      const data = await response.json();
      if (!response.ok) throw new Error(data.message || 'Netwerkverzoek mislukt');
      return data;
    }
    async function refresh() {
      try {
        const data = await api('/api/network');
        document.getElementById('network-ssid').textContent = data.ssid || 'Niet verbonden';
        document.getElementById('network-current').textContent = data.bssid ? `${data.bssid} (${data.rssi} dBm)` : 'Niet verbonden';
        document.getElementById('network-preferred').textContent = data.preferredBssid || 'Geen voorkeur';
        if (!initialized) { timeout.value = data.timeout; initialized = true; }
        if (data.scanning || Date.now() - scanRequestedAt > 2500) scanPending = false;
        if (selectionPending && Date.now() - selectionRequestedAt > 2500) {
          selectionPending = false;
          controls.disabled = false;
        }
        const networks = data.networks || [];
        const rows = JSON.stringify([networks, data.ssid, data.preferredBssid]);
        if (rows !== lastRows) {
          const selected = list.querySelector('input:checked')?.value || data.preferredBssid;
          list.replaceChildren();
          for (const network of networks.sort((first, second) => second.rssi - first.rssi)) {
            const row = document.createElement('tr');
            const choice = document.createElement('td');
            const radio = document.createElement('input');
            radio.type = 'radio'; radio.name = 'bssid'; radio.value = network.bssid;
            radio.disabled = network.ssid !== data.ssid || !data.bssid;
            radio.checked = !radio.disabled && network.bssid === selected;
            radio.setAttribute('aria-label', `${network.ssid || 'Verborgen netwerk'} ${network.bssid}`);
            radio.title = radio.disabled ? 'Ander netwerk' : network.bssid;
            radio.addEventListener('change', () => { save.disabled = busy || selectionPending; });
            choice.append(radio); row.append(choice);
            const name = document.createElement('td');
            const label = document.createElement('label');
            radio.id = `ap-${network.bssid.replaceAll(':', '')}`;
            label.htmlFor = radio.id;
            label.append(document.createTextNode(network.ssid || 'Verborgen netwerk'), document.createElement('br'), document.createTextNode(network.bssid));
            name.append(label); row.append(name);
            const signal = document.createElement('td');
            signal.textContent = `${network.rssi} dBm`;
            row.append(signal);
            const channel = document.createElement('td'); channel.textContent = network.channel;
            row.append(channel); list.append(row);
          }
          lastRows = rows;
        }
        busy = data.scanning || scanPending;
        scan.disabled = busy || selectionPending;
        save.disabled = busy || selectionPending || !list.querySelector('input:checked:not(:disabled)');
        message.textContent = busy ? 'Wifi-scan bezig...' : (data.error || data.message || (networks.length ? '' : 'Geen scanresultaten'));
      } catch (error) {
        message.textContent = 'Radio tijdelijk niet bereikbaar. Opnieuw verbinden...';
      } finally {
        window.setTimeout(refresh, 1500);
      }
    }
    scan.addEventListener('click', async () => {
      scan.disabled = true;
      try {
        await api('/api/network/scan', {method: 'POST'});
        scanPending = true; scanRequestedAt = Date.now();
        message.textContent = 'Wifi-scan aangevraagd...';
      } catch (error) { message.textContent = error.message; scan.disabled = false; }
    });
    document.getElementById('network-form').addEventListener('submit', async event => {
      event.preventDefault();
      const selected = list.querySelector('input:checked:not(:disabled)');
      if (!selected || busy) return;
      const body = new URLSearchParams({bssid: selected.value, timeout: timeout.value});
      controls.disabled = true;
      try {
        const data = await api('/api/network/select', {method: 'POST', body});
        selectionPending = true; selectionRequestedAt = Date.now();
        message.textContent = data.message;
      } catch (error) { message.textContent = error.message; controls.disabled = false; }
    });
    refresh();
  })();
  </script>)JS";
  request->send(200, "text/html", getHtmlStart() + setHtmlBody(body, script) + getHtmlEnd());
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
    <a href='/netwerk'>Netwerk</a>
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