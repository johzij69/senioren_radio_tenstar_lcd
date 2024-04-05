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
            { request->send_P(200, "image/x-icon", favicon, sizeof(favicon)); });

  server.on("/api/streams", HTTP_GET, std::bind(&PrioWebServer::handleApi, this, std::placeholders::_1));

  server.on("/", HTTP_GET, std::bind(&PrioWebServer::handleRoot, this, std::placeholders::_1));
  server.on("/inpustream", HTTP_GET, std::bind(&PrioWebServer::handleInputStream, this, std::placeholders::_1));

  server.onRequestBody([this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                       {
                         if (request->url() == "/updateurls")
                         {
                           this->handleSettings(request, data);
                           request->send(200, "text/plain", "true");
                         }

                         if (request->url() == "/addstream")
                         {
                           this->handleAddStream(request, data);
                           request->send(200, "text/plain", "true");
                         } });

  server.begin();
}
/* send large files chunked as example */
/* result is the same as using send_p for large ebpages from PROGMEN */

// void PrioWebServer::handleRoot(AsyncWebServerRequest *request)
// {
//   Serial.println("present root page");

//   //  for (size_t i = 20; i < urlManager.streamCount; i++) {
//   //    urlManager.deleteStream(i);
//   //  }

//   bigString.reserve(40000);

//   String htmlStart PROGMEM = getHtmlStart();
//   String htmStreamList PROGMEM = getEditUrlContainer();
//   String htmlEnd PROGMEM = getHtmlEnd();

//   //  htmlPage.reserve(105000);

//   // // Serial.println(htmStreamList);
//   // String body PROGMEM = R"(<form method="post" id="updateForm" action="/updateurls">)";
//   // body += htmStreamList;
//   // body += R"( <input class="input_button" type="button" value="Bijwerken" onclick='toggleEdit()'/>)";
//   // body += "</form>";

//   bigString.concat(htmlStart);
//   // bigString.concat(htmStreamList);
//   bigString.concat(htmlEnd);

//   Serial.println(bigString.length());
//   // Serial.println(bigString);

//   AsyncWebServerResponse *response = request->beginChunkedResponse("text/html", [this](uint8_t *buffer, size_t maxlen, size_t index) -> size_t
//                                                                    {

//     size_t len = min(maxlen, this->bigString.length() - index);
//     Serial.printf("Max %u bytes\n", maxlen);
//     Serial.printf("Sending %u bytes\n", len);
//     Serial.printf("index start\n %u ", index);
//     Serial.println(this->bigString.substring(index,index+len));

//     memcpy(buffer, this->bigString.c_str() + index, len);
//     return len; });

//   //  response->addHeader("Server", "ESP Async Web Server");
//   request->send(response);

// }

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
    AsyncWebParameter *p = request->getParam(i);

    if (p->name() == "size")
    {
      page_size = p->value().toInt();
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

  Serial.println("total_pages:" + String(total_pages));

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
    JsonObject data_0 = data.add<JsonObject>();
    data_0["id"] = i;
    data_0["name"] = urlManager.Streams[i].name;
    data_0["url"] = urlManager.Streams[i].url;
    data_0["logo"] = urlManager.Streams[i].logo;
  }

  String response;

  doc.shrinkToFit(); // optional

  serializeJson(doc, response);

  request->send(200, "application/json", response);
}

void PrioWebServer::handleRoot(AsyncWebServerRequest *request)
{
  Serial.println("present root page");
  Serial.print("# of streams:");
  Serial.println(urlManager.streamCount);

  int paramsNr = request->params();
  // Serial.println(paramsNr);

  int start = 0;
  int size = 5;

  for (int i = 0; i < paramsNr; i++)
  {
    AsyncWebParameter *p = request->getParam(i);

    if (p->name() == "start")
    {
      start = p->value().toInt();
    }

    if (p->name() == "size")
    {
      size = p->value().toInt();
    }
  }

  int end = start + size;
  // bigString.reserve(90000);

  if (start < 0 || start > urlManager.streamCount)
  {
    start = 0;
  }
  if (end < 0 || end > urlManager.streamCount)
  {
    end = start + 5;

    if (end >= urlManager.streamCount)
    {
      end = urlManager.streamCount - 1;
    }
  }

  Serial.print("Start:");
  Serial.println(String(start));
  Serial.print("Eind:");
  Serial.println(String(end));

  String htmlStart PROGMEM = getHtmlStart();
  String htmStreamList PROGMEM = getStreamList(start, end);
  String htmlEnd PROGMEM = getHtmlEnd();

  String saver PROGMEM = R"(<div id="saving"><div class="loader"></div></div>)";
  String pager_i PROGMEM = "<div><h2>Showing stream " + String(start + 1) + "-" + String(end) + "</h2></div>";
  String form_s PROGMEM = R"(<form method="post" id="updateForm" action="/updateurls">)";
  String input PROGMEM = R"( <input class="input_button" type="button" value="Opslaan" onclick='saveStreams()'/>)";
  String next PROGMEM = "<input class=\"input_button\" type=\"button\" value=\">\" onclick='nextPage(" + String(end) + "," + String(size) + ")'/>";
  String form_e PROGMEM = R"(</form>)";

  bigString.concat(htmlStart);
  bigString.concat(saver);
  bigString.concat(pager_i);
  bigString.concat(form_s);
  bigString.concat(htmStreamList);
  bigString.concat(input);

  if (end < urlManager.streamCount)
  {
    Serial.println("jj");
    bigString.concat(next);
  }
  bigString.concat(form_e);
  bigString.concat(htmlEnd);
  bigString.concat('\0');

  request->send_P(200, "text/html", this->bigString.c_str());
  bigString = "";
}

void PrioWebServer::handleAddStream(AsyncWebServerRequest *request, uint8_t *data)
{
  Serial.println("addstream.....");
  // JsonDocument doc;
  // DeserializationError error = deserializeJson(doc, data);
  // serializeJson(doc, Serial);
  urlManager.addStream(data);
}

void PrioWebServer::handleInputStream(AsyncWebServerRequest *request)
{
  Serial.println("present input stream page");

  // htmlPage.reserve(45000);

  String content PROGMEM = R"(
      <div class="content">
      <form method="post" id="updateForm">
        <div id="saving"><div class="loader"></div></div>
        <div class="stream_item">
          <div id="naam" class="naam-container">
            <div class="edit-label">Stream naam:</div>
            <input
              class="input_naam"
              id="input_naam"
              type="text"
              name="naam"
              value=""
            />
          </div>
          <div id="url-0" class="url-container">
            <div class="edit-label">Stream url:</div>
            <input
              class="input_url"
              id="input_url"
              type="text"
              name="url"
              value=""
            />
          </div>
          <div id="url-log-0" class="url-container">
            <div class="edit-label">Logo url:</div>
            <input
              class="input_url"
              id="input_logo"
              type="text"
              name="logo"
              value=""
            />
          </div>
          <input
            id="url_button"
            class="input_button"
            type="button"
            value="Opslaan"
            onclick='saveStream()'
          />
        </div>
      </form>
    </div>)";

  this->htmlPage = createHtmlPage(content);
  request->send_P(200, "text/html", this->htmlPage.c_str());
}

void PrioWebServer::handleSettings(AsyncWebServerRequest *request, uint8_t *data)
{

  Serial.println("saving streams");

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data);
  serializeJson(doc, Serial);

  urlManager.saveStreams(data);

  // if (error)
  // {
  //   Serial.print("deserializeJson() returned ");
  //   Serial.println(error.c_str());
  //   //    return false;
  // }
  // int index = 0; // Teller voor de index

  // for (JsonPair item : doc.as<JsonObject>())
  // {
  //   const char *item_key = item.key().c_str(); // "container-0", "container-1", "container-2", ...
  //   int index = atoi(item_key + strlen("container-"));
  //   //    const char *value_stream_id = item.value()["stream_id"];   // "0", "1", "2", "3", "4", "5", "6"
  //   const char *value_stream_url = item.value()["url"]; // "https://icecast.omroep.nl/radio1-bb-mp3", ...
  //   const char *value_logo_url = item.value()["logo"];

  //   urlManager.urls[index] = value_stream_url;
  //   urlManager.logo_urls[index] = value_logo_url;
  //   urlManager.saveUrls();

  //   // return true;
  // }
}

void PrioWebServer::handleAddUrl(AsyncWebServerRequest *request)
{
  String body PROGMEM = "";
  body += "<form method='post' action='/updateurls'>";
  body += "Nieuwe URL: <input class='input_url' type='text' name='newurl'><br>";
  body += "Nieuwe URL-logo: <input class='input_url_logo' type='text' name='newurl_logo'><br>";
  body += "<input  class='input_button' type='submit' value='Bijwerken'>";
  body += "</form>";
  request->send(200, "text/html", createHtmlPage(body));
}

String PrioWebServer::getStreamList(int start, int end)
{

  String PROGMEM htmlOutput = "";

  for (size_t i = start; i < end; i++)
  {

    htmlOutput += R"(
        <div id="container-@url_index" class="item-container">
          <div id="@url_index" class="stream_item" draggable="true">
            <div id="naam-@url_index" class="naam-container">
              <div class="edit-label">Stream naam:</div>
              <input
                class="input_naam"
                type="text"
                name="naam-@url_index"
                value="@stream_naam"
              />
            </div>
            <div id="url-@url_index" class="url-container">
              <div class="edit-label">Stream url:</div>
              <input
                class="input_url"
                type="text"
                name="newurl"
                value="@url_item_url"
              />
            </div>
            <div id="url-log-@url_index" class="url-container">
              <div class="edit-label">Logo url:</div>
              <input
                class="input_url"
                type="text"
                name="newurl_logo"
                value="@url_item_logo"
              />
            </div>
          </div>
        </div>
      )";

    searchAndReplace(&htmlOutput, String("@url_index"), String(i));
    searchAndReplace(&htmlOutput, String("@url_item_url"), urlManager.Streams[i].url);
    searchAndReplace(&htmlOutput, String("@url_item_logo"), urlManager.Streams[i].logo);
    searchAndReplace(&htmlOutput, String("@stream_naam"), urlManager.Streams[i].name);
  }

  return htmlOutput;
}

void PrioWebServer::deleteStreamItem(AsyncWebServerRequest *request)
{
  String body PROGMEM = "";
  for (size_t i = 0; i < urlManager.urls.size(); i++)
  {
    body += "<form action='deleteurl' method='post'>";
    body += "<p>";
    body += "<label for='url'" + String(i) + ">URL" + String(i) + ":</label>";
    body += "<span>";
    body += urlManager.urls[i];
    body += "</span>";
    body += "<button class='input_button' type='submit' name='urlindex' value='" + String(i) + "'>X</button>";
    body += "</p>";
    body += "</form>";
  }

  request->send(200, "text/html", createHtmlPage(body));
}

String PrioWebServer::createHtmlPage(String html)
{

  String page PROGMEM = getHtmlStart();
  page += html;
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
  html += getScript();
  html += "</head>";
  html += "<body>";
  html += getTopMenu();
  html += "<div class='content'>";
  return html;
}

String PrioWebServer::getHtmlEnd()
{
  String endHtml PROGMEM = "";
  endHtml += "</div>";
  endHtml += "</body>";
  endHtml += "</html>";
  return endHtml;
}

String PrioWebServer::getTopMenu()
{

  String html PROGMEM = R"(
  <div class='top-menu'>
    <a href='/'>Overzicht</a>
    <a href='/deleteurl'>Verwijder</a>
    <a href='/inpustream'>Voeg toe</a>
    <a href='#'>Contact</a>
  </div>
  )";

  return html;
}

void PrioWebServer::searchAndReplace(String *htmlString, String findPattern, String replaceWith)
{
  int index = 0;
  while ((index = htmlString->indexOf(findPattern, index)) != -1)
  {
    htmlString->replace(findPattern, replaceWith);
    // Update index to search for next occurrence
    index += replaceWith.length();
  }
}