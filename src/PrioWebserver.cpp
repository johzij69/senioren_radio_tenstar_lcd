#include "PrioWebServer.h"

PrioWebServer::PrioWebServer(UrlManager &urlManager, int port)
    : urlManager(urlManager), server(port)
{
}

void PrioWebServer::begin()
{

  // Route definities
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "image/x-icon", favicon, sizeof(favicon)); });

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
  // server.on("/showurls", HTTP_GET, [this](AsyncWebServerRequest *request)
  //           {
  //           Serial.println("getting urls from prferences:");
  //           urlManager.printAllUrls();

  //       String urlsAsString = urlManager.CreateDivUrls();
  //       Serial.println(urlsAsString);
  //       request->send(200, "text/html", urlsAsString); });

  server.on("/updateurls", HTTP_POST, [this](AsyncWebServerRequest *request)
            {
              String newurl;
              String newurl_logo;
              if (request->hasParam("newurl", true))
              {
                newurl = request->getParam("newurl", true)->value();
                
                if (request->hasParam("newurl_logo", true)) {
                  newurl_logo = request->hasParam("newurl_logo", true);
                  if(newurl_logo != "") {
                    urlManager.addUrl(newurl.c_str(), newurl_logo.c_str());
                  }
                } else {
                    urlManager.addUrl(newurl.c_str());
                }
                
  
                request->redirect("/");
              } });
  // server.on("/showprefs", HTTP_GET, [this](AsyncWebServerRequest *request)
  //           {
  //       Serial.println("getting urls from prferences:");
  //       urlManager.PrinturlsFromPrev();
  //       request->send(200, "text/html", "see console"); });

  // Begin de server
  server.begin();
}

void PrioWebServer::handleAddUrl(AsyncWebServerRequest *request)
{
  String body = "";
  body += "<form method='post' action='/updateurls'>";
  body += "Nieuwe URL: <input class='input_url' type='text' name='newurl'><br>";
  body += "Nieuwe URL-logo: <input class='input_url_logo' type='text' name='newurl_logo'><br>";
  body += "<input  class='input_button' type='submit' value='Bijwerken'>";
  body += "</form>";
  request->send(200, "text/html", createHtmlPage(body));
}

void PrioWebServer::handleRoot(AsyncWebServerRequest *request)
{
  String body = getEditUrlContainer();
  // for (size_t i = 0; i < urlManager.urls.size(); i++)
  // {
  //   body += "<div id=\"" + String(i) + "\"><p>URL " + String(i) + ": " + urlManager.urls[i] + "</p></div>";
  // }
  request->send(200, "text/html", createHtmlPage(body));
}

String PrioWebServer::getEditUrlContainer()
{

  String htmlOutput = "";
  for (size_t i = 0; i < urlManager.urls.size(); i++)
  {
    htmlOutput += R"(      
        <div id="@url_index" class="stream_item">
        <form method="post" action="/updateurls">
          <div id="url-@url_index" class="url-container">
            <div class="edit-label">Stream url:</div>
            <input
              readonly
              class="input_url"
              type="text"
              name="newurl"
              value="@url_item_url"
            /><br />
          </div>
          <div id="url-log-@url_index" class="url-container">
            <div class="edit-label">Logo url:</div>
            <input
              readonly
              class="input_url"
              type="text"
              name="newurl_logo"
              value="@url_item_logo"
            /><br />
          </div>
          <input
            class="input_button align-right"
            type="button"
            value="Bijwerken"
            onclick='toggleEdit()'
          />
        </form>
      </div>
      )";
    searchAndReplace(&htmlOutput, String("@url_index"), String(i));
    searchAndReplace(&htmlOutput, String("@url_item_url"), urlManager.urls[i]);
    searchAndReplace(&htmlOutput, String("@url_item_logo"), urlManager.logo_urls[i]);
  }
  return htmlOutput;
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
    body += "<button class='input_button' type='submit' name='urlindex' value='" + String(i) + "'>X</button>";
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
  String endHtml = "";
  endHtml += "</div>";
  endHtml += "</body>";
  endHtml += "</html>";
  return endHtml;
}

String PrioWebServer::getTopMenu()
{

  String html = "";
  html += "<div class='top-menu'>";
  html += "<a href='/'>Overzicht</a>";
  html += "<a href='/deleteurl'>Verwijder</a>";
  html += "<a href='/addurl'>Voeg toe</a>";
  html += "<a href='#'>Contact</a>";
  html += "</div>";

  return html;
}

String PrioWebServer::getStyling()
{
  String Style = R"(<style>
      body {
        margin: 0;
        font-family: Arial, sans-serif;
      }
      .content {
        margin-top: 10px;
        margin-left: 10px;
        font-family: Arial, sans-serif;
      }
      .content input {
        padding: 5px;
        font-family: Arial, sans-serif;
      }
      .input_url {
        padding: 5px;
        width: 350px;
        border-radius: 5px;
        border-width: thin;
        margin-left: 5px;
        margin-right: 5px;
      }
      .input_button {
        padding: 5px;
        border-radius: 5px;
        border-width: thin;
        padding-left: 10px;
        padding-right: 10px;
        margin-left: 10px;
      }
      .top-menu {
        background-color: #333;
        overflow: hidden;
      }
      .top-menu a {
        float: left;
        display: block;
        color: white;
        text-align: center;
        padding: 14px 16px;
        text-decoration: none;
      }
      .top-menu a:hover {
        background-color: #ddd;
        color: black;
      }
      @media screen and (max-width: 600px) {
        .top-menu a {
          float: none;
          display: block;
          width: 100%;
          text-align: left;
        }
      }
      .stream_item {
        padding: 5px;
        padding-left: 15px;
        border: 2px solid lightgray;
        border-radius: 8px;
        margin-bottom: 5px;
        max-width: 380px;
        padding-bottom: 50px;
      }
      .edit-label {
        margin-top: 5px;
        margin-left: 7px;
        font-size: 14px;
        font-weight: bold;
        margin-bottom: 2px;
      }
      .align-right {
        float: right;
        margin-right: 15px;
        margin-top: 15px;
      }
      .url-container {
        margin-top: 15px;
      }
    </style>
    )";
  return Style;
}

String PrioWebServer::getScript()
{
  String script = R"(<script>
      function toggleEdit() {
        var inputs = document.querySelectorAll('.input_url');
        var button = document.querySelector('.input_button');

        if (button.value === 'Bijwerken') {
          inputs.forEach(function(input) {
            input.removeAttribute('readonly');
          });
          button.value = 'Opslaan';
          document.getElementById('updateForm').setAttribute('onsubmit', 'submitForm(); return false;');
        } else {
          inputs.forEach(function(input) {
            input.setAttribute('readonly', true);
          });
          button.value = 'Bijwerken';
          document.getElementById('updateForm').removeAttribute('onsubmit');
        }
      }

      function submitForm() {
        document.getElementById('updateForm').submit();
      }
    </script>)";

  return script;
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