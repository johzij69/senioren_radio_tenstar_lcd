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
    }
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send_P(200, "image/x-icon", favicon, sizeof(favicon));
  });

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
    }
  });

  server.begin();
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
  String body = R"(
    <div class="content">
      <h2>Radio Streams</h2>
      <div class="container">
        <div class="pagination">
          <button id="prevBtn" disabled>Previous</button>
          <span id="currentPage">Page 1</span>
          <button id="nextBtn">Next</button>
        </div>
        <span class="pagesize-container">
          <label for="pagesize">pagesize:</label>
          <select name="pagesize" id="pagesize">
            <option value="3">3</option>
            <option value="5" selected="selected">5</option>
            <option value="8">8</option>
            <option value="10">10</option>
          </select>
        </span>
      </div>
      <form method="post" id="updateForm">
        <div id="saving"><div class="loader"></div></div>
        <div id="streams"></div>
      </form>
      <span
        ><input
          class="input_button"
          type="button"
          value="Opslaan"
          onclick='saveStreams()' 
        />
      </span>
    </div>  
      )";


bigString = "";

String h_start PROGMEM = getHtmlStart();
String h_body PROGMEM = setHtmlBody(body);
String h_end PROGMEM = getHtmlEnd();

bigString.concat(h_start);
bigString.concat(h_body);
bigString.concat(h_end);


//  bigString = createHtmlPage(body);
  request->send_P(200, "text/html", this->bigString.c_str());
  bigString = "";
}

void PrioWebServer::handleAddStream(AsyncWebServerRequest *request, uint8_t *data)
{
  urlManager.addStream(data);
}

void PrioWebServer::handleInputStream(AsyncWebServerRequest *request)
{
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

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data);
  serializeJson(doc, Serial);
  urlManager.saveStreams(data);
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

String PrioWebServer::createHtmlPage(String body)
{

  String page PROGMEM = "";
  page += getHtmlStart();
  page += setHtmlBody(body);
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
    <a href='/deleteurl'>Verwijder</a>
    <a href='/inpustream'>Voeg toe</a>
    <a href='#'>Contact</a>
  </div>
  )";

  return html;
}

String PrioWebServer::setHtmlBody(String body)
{
  String html PROGMEM = "";
  html += "<body>";
  html += getTopMenu();
  html += body;
  html += getScript(this->ip);
  html += "</body>";
  return html;
}

String PrioWebServer::getHtmlEnd()
{
  String endHtml PROGMEM = "";
  endHtml += "</html>";
  return endHtml;
}