#include "PrioWebjavascript.h"

String putInstellingenSyncTimeButton(String ip)
 {
  String script PROGMEM = R"(
     <script>
      async function syncTime() {
        try {
          document.getElementById("syncing").style.display = "block";
          const url = "/api/synctime";
          const options = {
            method: "GET",
            headers: {
              "Content-Type": "application/json",
            },
          };
          const response = await fetch(url, options);
          if (!response.ok) {
            throw new Error("Network response was not ok");
          }
          document.getElementById("syncing").style.display = "none";
        } catch (error) {
          console.error("Er is een fout opgetreden:", error);
        }
      }
    </script>)";
//   searchAndReplace(&script, String("@ip"), ip);
   return script;
 }    

String getAddScript(String ip)
{
  String script PROGMEM = R"(
     <script>
      const streamContainer = document.getElementById("stream-container");
      const contentContainer = document.getElementById("content-container");
      async function saveStream() {
        try {
          document.getElementById("saving").style.display = "block";
          const formData = {};
          const streamNaam = document.getElementById("input_naam").value;
          const streamUrl = document.getElementById("input_url").value;
          const streamLogo = document.getElementById("input_logo").value;

          formData["json_settings"] = {
            name: streamNaam,
            url: streamUrl,
            logo: streamLogo,
          };

          // Maak een POST-verzoek met fetch
          const url = "/api/addstream";
          const options = {
            method: "POST",
            headers: {
              "Content-Type": "application/json",
            },
            body: JSON.stringify(formData),
          };

          const response = await fetch(url, options);
          if (!response.ok) {
            throw new Error("Network response was not ok");
          }

          document.getElementById("saving").style.display = "none";
        } catch (error) {
          console.error("Er is een fout opgetreden:", error);
        }
      }

      function displayStream() {
        contentContainer.innerHTML = "";
        const spanSubmit = document.createElement("span");
        spanSubmit.innerHTML = `<input
                            class="input_button"
                            type="button"
                            value="Opslaan"
                            onclick='saveStream()'
                          />`;
        const streamForm = document.createElement("form");
        streamForm.setAttribute("method", "post");
        streamForm.setAttribute("id", "updateForm");
        streamForm.innerHTML = `
                          <div id="saving"><div class="loader"></div></div>
                          <div class="stream_item">
                            <div id="naam" class="naam-container">
                              <div class="edit-label">Stream naam:</div>
                              <input
                                id="input_naam"
                                class="input_short"
                                type="text"
                                name="naam"
                                value=""
                              />
                            </div>
                            <div id="url" class="url-container">
                              <div class="edit-label">Stream url:</div>
                              <input
                                id="input_url"
                                class="input_long"
                                type="text"
                                name="newurl"
                                value=""
                              />
                            </div>
                            <div id="logo" class="url-container">
                              <div class="edit-label">Logo url:</div>
                              <input
                                id="input_logo"	
                                class="input_long"
                                type="text"
                                name="logo"
                                value=""
                              />
                            </div>
                        </div>
                     `;
        contentContainer.appendChild(streamForm);
        contentContainer.appendChild(spanSubmit);
      }
      displayStream();
          </script>)";
  searchAndReplace(&script, String("@ip"), ip);
  return script;
}

String getMainScript(String ip)
{

  String script PROGMEM = R"(
     <script>
      let currentPage = 1;
      let pagesize = 5;

      const requestOptions = {
        method: "GET",
        redirect: "follow",
      };

      const contentContainer = document.getElementById("content-container");
      contentContainer.innerHTML = "";

      const contentTitle = document.createElement("h2");
      contentTitle.textContent = "Radio Streams";
      contentContainer.appendChild(contentTitle);

      const pagingContainer = document.createElement("div");
      pagingContainer.setAttribute("class", "container");
      contentContainer.appendChild(pagingContainer);

      const paginationContainer = document.createElement("div");
      paginationContainer.setAttribute("class", "pagination");
      pagingContainer.appendChild(paginationContainer);

      const prevBtn = document.createElement("button");
      prevBtn.setAttribute("id", "prevBtn");
      prevBtn.setAttribute("class", "prio-mr5");
      prevBtn.textContent = "Previous";
      paginationContainer.appendChild(prevBtn);

      const currentPageSpan = document.createElement("span");
      currentPageSpan.setAttribute("id", "currentPage");
      currentPageSpan.textContent = `Page ${currentPage}`;
      paginationContainer.appendChild(currentPageSpan);

      const nextBtn = document.createElement("button");
      nextBtn.setAttribute("id", "nextBtn");
      nextBtn.setAttribute("class", "prio-ml5");
      nextBtn.textContent = "Next";
      paginationContainer.appendChild(nextBtn);

      prevBtn.addEventListener("click", () => {
        if (currentPage > 1) {
          fetchStreams(currentPage - 1, pagesize);
        }
      });

      nextBtn.addEventListener("click", () => {
        fetchStreams(currentPage + 1, pagesize);
      });

      const pagesizeContainer = document.createElement("span");
      pagesizeContainer.setAttribute("class", "pagesize-container");

      const pagesizeLabel = document.createElement("label");
      pagesizeLabel.setAttribute("for", "pagesize");
      pagesizeLabel.textContent = "Pagesize:";
      pagesizeContainer.appendChild(pagesizeLabel);
      const pagesizeSelect = document.createElement("select");
      pagesizeSelect.setAttribute("name", "pagesize");
      pagesizeSelect.setAttribute("id", "pagesize");
      pagesizeSelect.setAttribute("class", "prio-p2");
      pagesizeSelect.innerHTML = `<option value="3">3</option><option value="5" selected="selected">5</option><option value="8">8</option><option value="10">10</option><option value="all">all</option>`;
      pagesizeSelect.addEventListener("change", (event) => {
        if (pagesizeSelect.value != pagesize) {
          currentPage = 1;
          pagesize = pagesizeSelect.value;
          fetchStreams(currentPage, pagesize); 
        } 
      });
      pagesizeContainer.appendChild(pagesizeSelect);
      pagingContainer.appendChild(pagesizeContainer);

      const numberofStreams = document.createElement("span");
      numberofStreams.setAttribute("id", "numberofStreams");
      pagingContainer.appendChild(numberofStreams);

      const streamForm = document.createElement("form");
      streamForm.setAttribute("method", "post");
      streamForm.setAttribute("id", "updateForm");
      contentContainer.appendChild(streamForm);

      const saveAnimation = document.createElement("div");
      saveAnimation.setAttribute("id", "saving");

      const animationLoader = document.createElement("div");
      animationLoader.setAttribute("class", "loader");

      saveAnimation.appendChild(animationLoader);
      streamForm.appendChild(saveAnimation);

      const streamsContainer = document.createElement("div");
      streamsContainer.setAttribute("id", "streams");
      streamsContainer.innerHTML = "";
      streamForm.appendChild(streamsContainer);

      const spanSubmit = document.createElement("span");
      spanSubmit.innerHTML = `<input
                            class="input_button"
                            type="button"
                            value="Opslaan"
                            onclick='saveStreams()'
                          />`;
      streamForm.append(spanSubmit);

      async function fetchStreams(page, size) {
        const currentPageSpan = document.getElementById("currentPage");
        const nextBtn = document.getElementById("nextBtn");
        const prevButton = document.getElementById("prevBtn");

        const response = await fetch(
          `http://@ip/api/streams?page=${page}&size=${size}`,
          requestOptions
        );
        if (response.ok) {
          const data = await response.json();
          displayStreams(data.data);

          const nuberOfStreamsSpan = document.getElementById("numberofStreams");
          nuberOfStreamsSpan.textContent = `${data.total_streams} Streams`;
          
          currentPage = page;
          currentPageSpan.textContent = `Page ${currentPage}`;
          if (currentPage === 1) {
            prevButton.disabled = true;
          } else {
            prevButton.disabled = false;
          }
          if (data.total_pages === currentPage) {
            nextBtn.disabled = true;
          } else {
            nextBtn.disabled = false;
          }
        }
      }

      function displayStreams(streams) {
        const streamsContainer = document.getElementById("streams");
        streamsContainer.setAttribute("id", "streams");
        streamsContainer.innerHTML = "";

        streams.forEach((stream) => {
          const streamContainer = document.createElement("div");
          streamContainer.setAttribute("id", `container-${stream.id}`);
          streamContainer.setAttribute("class", "item-container");
          streamContainer.addEventListener("dragover", handleDragOver);
          streamContainer.addEventListener("drop", handleDrop);
          streamContainer.addEventListener("dragleave", dragLeave);

          const streamItem = document.createElement("div");
          streamItem.setAttribute("id", stream.id);
          streamItem.setAttribute("class", "stream_item");
          streamItem.setAttribute("draggable", "true");
          streamItem.addEventListener("dragstart", handleDragStart);
          streamItem.innerHTML = `
                 <div id="naam-${stream.id}" class="naam-container">
                    <div class="edit-label">Stream naam:</div>
                    <input
                      class="input_short input_naam"
                      type="text"
                      name="naam-${stream.id}"
                      value="${stream.name}"
                    />
                    <div class="delete-icon-container" onclick='deleteStream(${stream.id})'>
                      <div class="delete-icon"></div> 
                    </div>
                  </div>
                  <div id="url-${stream.id}" class="url-container">
                    <div class="edit-label">Stream url:</div>
                    <input
                      class="input_long input_url"
                      type="text"
                      name="newurl-${stream.id}"
                      value="${stream.url}"
                    />
                  </div>
                  <div id="url-log-${stream.id}" class="url-container">
                    <div class="edit-label">Logo url:</div>
                    <input
                      class="input_long input_logo"
                      type="text"
                      name="newurl_logo_${stream.id}"S
                      value="${stream.logo}"
                    />
                  </div>
               `;
          streamContainer.appendChild(streamItem);
          streamsContainer.appendChild(streamContainer);
        });
      }

      async function saveStreams() {
        try {
          // Verzamel de gegevens van het formulier
          document.getElementById("saving").style.display = "block";

          const formData = {};
          const containers = document.querySelectorAll(".item-container");
          containers.forEach((container) => {
            const containerId = container.getAttribute("id");
            const streamItem = container.querySelector(".stream_item");
            const streamId = streamItem.getAttribute("id");
            const streamNaam = streamItem.querySelector(".input_naam").value;
            const streamUrl = streamItem.querySelector(".input_url").value;
            const streamLogo = streamItem.querySelector(".input_logo").value;

            formData[containerId] = {
              id: streamId,
              name: streamNaam,
              url: streamUrl,
              logo: streamLogo,
            };
          });

          // Maak een POST-verzoek met fetch
          const url = "/updateurls";
          const options = {
            method: "POST",
            headers: {
              "Content-Type": "application/json",
            },
            body: JSON.stringify(formData),
          };

          const response = await fetch(url, options);
          if (!response.ok) {
            throw new Error("Network response was not ok");
          }

          document.getElementById("saving").style.display = "none";
        } catch (error) {
          console.error("Er is een fout opgetreden:", error);
        }
      }
      
      async function deleteStream(id) {
        const response = await fetch(
          `http://@ip/api/deletestream?id=${id}`,
          requestOptions
        );
        if (response.ok) {
          fetchStreams(currentPage, pagesize);
        }
      }

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

      fetchStreams(currentPage, pagesize);
    </script>)";
  searchAndReplace(&script, String("@ip"), ip);
  return script;
}