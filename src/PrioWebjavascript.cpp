#include "PrioWebjavascript.h"

String getScript(String ip)
{

    String script PROGMEM = R"(
     <script>
          const streamsContainer = document.getElementById("streams");
          const prevButton = document.getElementById("prevBtn");
          const nextButton = document.getElementById("nextBtn");
          const currentPageSpan = document.getElementById("currentPage");
          const pagesizeSelect =  document.getElementById("pagesize");
          let currentPage = 1;
          let pagesize = 5

          const requestOptions = {
            method: "GET",
            redirect: "follow",
          };

          async function fetchStreams(page, size) {
            const response = await fetch(
              `http://@ip/api/streams?page=${page}&size=${size}`,
              requestOptions
            );
            console.log(response);
            if (response.ok) {
              const data = await response.json();
              displayStreams(data.data);
              currentPage = page;
              currentPageSpan.textContent = `Page ${currentPage}`;
              if (currentPage === 1) {
                prevButton.disabled = true;
              } else {
                prevButton.disabled = false;
              }
              if (data.total_pages === currentPage) {
                nextButton.disabled = true;
              } else {
                nextButton.disabled = false;
              }
            }
          }

          function displayStreams(streams) {
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
                      class="input_naam"
                      type="text"
                      name="naam-${stream.id}"
                      value="${stream.name}"
                    />
                  </div>
                  <div id="url-${stream.id}" class="url-container">
                    <div class="edit-label">Stream url:</div>
                    <input
                      class="input_url"
                      type="text"
                      name="newurl-${stream.id}"
                      value="${stream.url}"
                    />
                  </div>
                  <div id="url-log-${stream.id}" class="url-container">
                    <div class="edit-label">Logo url:</div>
                    <input
                      class="input_url"
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

          async function saveStream() {
            try {
              // Verzamel de gegevens van het formulier
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
              const url = "/addstream";
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
                const streamUrl = streamItem.querySelector(".input_url").value;
                const streamNaam = streamItem.querySelector(".input_naam").value;
                const logoUrl = streamItem.querySelector(
                  '.input_url[name^="newurl_logo"]'
                ).value;
                formData[containerId] = {
                  id: streamId,
                  name: streamNaam,
                  url: streamUrl,
                  logo: logoUrl,
                };
              });

              // Maak een POST-verzoek met fetch
              const url = "http://@ip/updateurls";
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

          prevButton.addEventListener("click", () => {
            if (currentPage > 1) {
              fetchStreams(currentPage - 1,pagesize);
            }
          });

          nextButton.addEventListener("click", () => {
            fetchStreams(currentPage + 1, pagesize);
          });

          pagesizeSelect.addEventListener("change", () => {
          if(pagesizeSelect.value != pagesize)
           {
              currentPage = 1;
              pagesize = pagesizeSelect.value;
              fetchStreams( currentPage, pagesize);
            }
          });
          fetchStreams(currentPage,pagesize);
    </script>)";
    searchAndReplace(&script, String("@ip"), ip);
    return script;
}