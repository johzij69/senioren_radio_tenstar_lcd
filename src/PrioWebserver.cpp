#include "PrioWebServer.h"

PrioWebServer::PrioWebServer(UrlManager &urlManager, int port)
    : urlManager(urlManager), server(port)
{
}

void PrioWebServer::begin()
{

  // Route definities

  // server.on("/addurl", HTTP_GET, std::bind(&PrioWebServer::handleAddUrl, this, std::placeholders::_1));

  // server.on("/deleteurl", HTTP_GET, [this](AsyncWebServerRequest *request)
  //           { this->deleteStreamItem(request); });

  // server.on("/deleteurl", HTTP_POST, [this](AsyncWebServerRequest *request)
  //           {
  //             String index;
  //             if (request->hasParam("urlindex", true))
  //             {
  //               index = request->getParam("urlindex", true)->value();
  //               urlManager.deleteUrl(index.toInt());
  //               request->redirect("/showurls");
  //             }
  //             else
  //             {
  //               request->send(200, "text/plain", "Url not found");
  //             } });

  // server.on("/updateurls", HTTP_POST, [this](AsyncWebServerRequest *request)
  //           {
  //             String json_settings;

  //             Serial.println("johan dit werkt niet");

  //             int params = request->params();

  //             Serial.println(String(params));

  //             // for (int i = 0; i < params; i++)
  //             // {
  //             //   AsyncWebParameter *p = request->getParam(i);
  //             //   if (p->isPost())
  //             //   {
  //             //     Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
  //             //   }
  //             // }

  //             if (request->hasParam("json_settings", true))
  //             {
  //               json_settings = request->getParam("json_settings", true)->value();

  //               Serial.println(json_settings);
  //             }
  //             request->send(200, "text/plain", json_settings);
  //             //  request->redirect("/");
  //           });
  // server.on("/addurl", HTTP_GET, std::bind(&PrioWebServer::handleAddUrl, this, std::placeholders::_1));
  // server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "image/x-icon", favicon, sizeof(favicon)); });

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
                            
                               JsonDocument doc;
   DeserializationError error = deserializeJson(doc, data);
  serializeJson(doc, Serial);
                            urlManager.addStream(data);
                            request->send(200, "text/plain", "true");
                         }
                         
                         
                          });

  server.begin();
}
/* send large files chunked as example */
/* result is the same as using send_p for large ebpages from PROGMEN */

// void PrioWebServer::handleRoot(AsyncWebServerRequest *request)
// {
//   Serial.println("present root page");

//   // htmlPage.reserve(45000);
//   String body PROGMEM = R"(<div class="content"><form method="post" id="updateForm" action="/updateurls">)";
//   body += getEditUrlContainer();
//   body += R"( <input class="input_button" type="button" value="Bijwerken" onclick='toggleEdit()'/>)";
//   body += "</form></div>";
//   this->htmlPage = createHtmlPage(body);
//   Serial.println(this->htmlPage.length());
//   AsyncWebServerResponse *response = request->beginChunkedResponse("text/html", [this](uint8_t *buffer, size_t maxlen, size_t index) -> size_t
//                                                                    {
//     size_t len = min(maxlen, this->htmlPage.length() - index);
//     Serial.printf("Sending %u bytes\n", len);
//     Serial.printf("index start\n %u ", index);
// //   Serial.println(this->htmlPage.substring(index,index+len));
//    memcpy(buffer, this->htmlPage.c_str() + index, len);
//     return len; });

//   response->addHeader("Server", "ESP Async Web Server");
//   request->send(response);
// }

void PrioWebServer::handleRoot(AsyncWebServerRequest *request)
{
  Serial.println("present root page");

  // htmlPage.reserve(45000);
  String body PROGMEM = R"(<div class="content"><form method="post" id="updateForm" action="/updateurls">)";
  body += getEditUrlContainer();
  body += R"( <input class="input_button" type="button" value="Bijwerken" onclick='toggleEdit()'/>)";
  body += "</form></div>";
  this->htmlPage = createHtmlPage(body);
  request->send_P(200, "text/html", this->htmlPage.c_str());
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

String PrioWebServer::getEditUrlContainer()
{

  String htmlOutput PROGMEM = "";
  for (size_t i = 0; i < urlManager.streamCount; i++)
  {
    htmlOutput += R"(      
        <div id="saving"><div class="loader"></div></div>
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


    Serial.println(urlManager.Streams[i].name);
    Serial.println(urlManager.Streams[i].url);
    Serial.println(urlManager.Streams[i].logo);  
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

String PrioWebServer::getStyling()
{
  String Style PROGMEM = R"(<style>
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
        width: 50%;
        border-radius: 5px;
        border-width: thin;
        margin-left: 5px;
        margin-right: 5px;
        width: 48vh;
      }

      .input_naam{
        padding: 5px;
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
        margin-left: 45vh;
        margin-top: 10px;
      }

      #saving {
        position: absolute;
        top: 45px;
        left: 0px;
        width: 100%;
        height: 100%;
        background-color: rgba(255, 255, 255, 0.5);
        display: none;
        z-index: 999;
      }

      .loader {
        position: absolute;
        left: 50vw;
        border: 8px solid #f3f3f3;
        border-top: 8px solid #3498db;
        border-radius: 50%;
        width: 50px;
        height: 50px;
        animation: spin 1s linear infinite;
        top: 50vh;
      }

      @keyframes spin {
        0% {
          transform: rotate(0deg);
        }
        100% {
          transform: rotate(360deg);
        }
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

      .stream_item {
        padding: 5px;
        padding-left: 15px;
        border: 2px solid lightgray;
        border-radius: 8px;
        margin-bottom: 5px;
        max-width: 52vh;
        padding-bottom: 24px;
        cursor: move; /* Add cursor style */
      }
      .edit-label {
        margin-top: 5px;
        margin-left: 7px;
        font-size: 14px;
        font-weight: bold;
        margin-bottom: 2px;
      }

      .url-container {
        margin-top: 15px;
      }

      .drop-zone {
        border: 2px dashed lightgray; /* Border style for drop zone */
        border-radius: 8px;
      }
      .drag-over {
        background-color: rgba(
          0,
          0,
          0,
          0.1
        ); /* Background color for drag-over */
      }

      @media screen and (max-width: 600px) {
        .top-menu a {
          float: none;
          display: block;
          width: 100%;
          text-align: left;
        }

        .stream_item {
          max-width: 31vh;
        }

        .input_url {
          width: 29vh;
        }

        .input_button {
          margin-left: 25vh;
        }
      }
    </style>
    )";
  return Style;
}

String PrioWebServer::getScript()
{

  String script PROGMEM = R"(<script>
      function saveStream() {
        // Verzamel de gegevens van het formulier
        document.getElementById("saving").style.display = "block";
        var formData = {};
        var streamNaam = document.getElementById("input_naam").value;
        var streamUrl = document.getElementById("input_url").value;
        var streamLogo = document.getElementById("input_logo").value;

        formData["json_settings"] = {
          name: streamNaam,
          url: streamUrl,
          logo: streamLogo,
        };

        // Converteer de gegevens naar JSON en geef deze een naam
        var postData = JSON.stringify(formData);

        var xhr = new XMLHttpRequest();
        xhr.open("POST", "/addstream", true);
        xhr.setRequestHeader("Content-Type", "application/json");
        xhr.onreadystatechange = function () {
          if (xhr.readyState === 4 && xhr.status === 200) {
            document.getElementById("saving").style.display = "none";
          }
        };
        xhr.send(JSON.stringify(postData));
      }

      function toggleEdit() {
        // Verzamel de gegevens van het formulier
        document.getElementById("saving").style.display = "block";

        var formData = {};
        var containers = document.querySelectorAll(".item-container");
        containers.forEach(function (container) {
          var containerId = container.getAttribute("id");
          var streamItem = container.querySelector(".stream_item");
          var streamId = streamItem.getAttribute("id");
          var streamUrl = streamItem.querySelector(".input_url").value;
          var streamNaam = streamItem.querySelector(".input_naam").value;
          var logoUrl = streamItem.querySelector('.input_url[name^="newurl_logo"]').value;
          formData[containerId] = {
            id: streamId,
            name: streamNaam,
            url: streamUrl,
            logo: logoUrl,
          };
        });

        // Converteer de gegevens naar JSON en geef deze een naam
        var postData = JSON.stringify(formData);


        // Doe hier iets met postData, bijvoorbeeld het naar de server sturen via een AJAX-verzoek
        // Voorbeeld met AJAX:
        var xhr = new XMLHttpRequest();
        xhr.open("POST", "/updateurls", true);
        xhr.setRequestHeader("Content-Type", "application/json");
        xhr.onreadystatechange = function () {
          if (xhr.readyState === 4 && xhr.status === 200) {
            document.getElementById("saving").style.display = "none";
          }
        };
        xhr.send(JSON.stringify(postData));
      }

      document.addEventListener("DOMContentLoaded", function () {
        // Stel event listeners in voor alle draggable elementen
        const draggables = document.querySelectorAll(".stream_item");
        draggables.forEach((draggable) => {
          draggable.addEventListener("dragstart", handleDragStart);
        });

        // Stel event listeners in voor alle dropzone containers
        const dropzones = document.querySelectorAll(".item-container");
        dropzones.forEach((dropzone) => {
          dropzone.addEventListener("dragover", handleDragOver);
          dropzone.addEventListener("drop", handleDrop);
          dropzone.addEventListener("dragleave", dragLeave);
        });
      });

      // Behandel dragstart event
      function handleDragStart(event) {
        // Identificeer het te verplaatsen element
        const draggedElement = event.target;
        const parentContainer = draggedElement.parentNode;

        // Stel data-transfer object in met element ID
        event.dataTransfer.setData("dragged_element_id", draggedElement.id);
        event.dataTransfer.setData("parentContainer", parentContainer.id);
      }

      // Behandel dragover event
      function handleDragOver(event) {
        // Voorkom standaard browsergedrag (zoals kopiëren)
        event.preventDefault();
        event.dataTransfer.dropEffect = "move";

        // indicator on stream _item
        const targetItem = event.target.closest(".stream_item");
        if (targetItem) {
          targetItem.classList.add("drag-over");
        }
      }
      // remove indicator drag-over
      function dragLeave(event) {
        const targetItem = event.target.closest(".stream_item");
        if (targetItem) {
          const rect = targetItem.classList.remove("drag-over");
        }
      }

      // Swap dragged item in container
      function handleDrop(event) {
        event.preventDefault();
        const orig_element = document.getElementById(this.id).children[0];
        const draggedElementId =
          event.dataTransfer.getData("dragged_element_id");
        const dragged_element = document.getElementById(draggedElementId);
        const sourceContainerId = event.dataTransfer.getData("parentContainer");
        const target_container = document.getElementById(this.id);
        const source_container = document.getElementById(sourceContainerId);
        target_container.innerHTML = "";
        target_container.appendChild(dragged_element);
        source_container.appendChild(orig_element);
        orig_element.classList.remove("drag-over");
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