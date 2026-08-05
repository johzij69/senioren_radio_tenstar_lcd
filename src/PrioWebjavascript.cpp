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
  String script PROGMEM = R"RAWSTR(
     <script>
      const streamContainer = document.getElementById("stream-container");
      const contentContainer = document.getElementById("content-container");
      let selectedFile = null;

      // Upload logo file to ESP32
      async function uploadLogo(streamName, file) {
        try {
          // Sanitize stream name for filename
          const sanitizedName = streamName.replace(/[^a-zA-Z0-9-_]/g, '_');
          const fileExtension = file.name.split('.').pop();
          const filename = `${sanitizedName}.${fileExtension}`;
          
          const formData = new FormData();
          formData.append('file', file, filename);
          
          const response = await fetch('/api/uploadlogo', {
            method: 'POST',
            body: formData
          });
          
          if (!response.ok) {
            throw new Error('Logo upload failed');
          }
          
          const result = await response.json();
          return result.path; // Returns /StreamLogos/filename
        } catch (error) {
          console.error('Error uploading logo:', error);
          throw error;
        }
      }

      async function saveStream() {
        try {
          document.getElementById("saving").style.display = "block";
          
          const streamNaam = document.getElementById("input_naam").value;
          const streamUrl = document.getElementById("input_url").value;
          let streamLogo = document.getElementById("input_logo").value;

          // If a file was selected, upload it first
          if (selectedFile) {
            try {
              streamLogo = await uploadLogo(streamNaam, selectedFile);
              console.log('Logo uploaded to:', streamLogo);
            } catch (error) {
              alert('Fout bij uploaden logo: ' + error.message);
              document.getElementById("saving").style.display = "none";
              return;
            }
          }

          const formData = {
            json_settings: {
              name: streamNaam,
              url: streamUrl,
              logo: streamLogo
            }
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

          alert('Stream succesvol toegevoegd!');
          document.getElementById("saving").style.display = "none";
          
          // Reset form
          document.getElementById("input_naam").value = '';
          document.getElementById("input_url").value = '';
          document.getElementById("input_logo").value = '';
          selectedFile = null;
          document.getElementById("file_info").textContent = 'Geen bestand geselecteerd';
          const preview = document.getElementById("logo_preview");
          if (preview) preview.style.display = 'none';
          
        } catch (error) {
          console.error("Er is een fout opgetreden:", error);
          alert('Fout bij opslaan: ' + error.message);
          document.getElementById("saving").style.display = "none";
        }
      }

      function handleFileSelect(event) {
        const file = event.target.files[0];
        if (file) {
          // Check file type
          if (!file.type.startsWith('image/')) {
            alert('Selecteer een afbeelding (JPG, PNG, GIF)');
            event.target.value = '';
            return;
          }
          
          // Check file size (max 200KB)
          if (file.size > 200 * 1024) {
            alert('Bestand te groot! Maximum 200KB');
            event.target.value = '';
            return;
          }
          
          selectedFile = file;
          document.getElementById("file_info").textContent = 
            `Geselecteerd: ${file.name} (${(file.size / 1024).toFixed(1)} KB)`;
          
          // Show preview
          const reader = new FileReader();
          reader.onload = function(e) {
            let preview = document.getElementById("logo_preview");
            if (!preview) {
              preview = document.createElement('img');
              preview.id = 'logo_preview';
              preview.className = 'logo-preview';
              document.querySelector('.file-upload-container').appendChild(preview);
            }
            preview.src = e.target.result;
            preview.style.display = 'block';
          };
          reader.readAsDataURL(file);
        }
      }

      function displayStream() {
        contentContainer.innerHTML = "";
        const spanSubmit = document.createElement("span");
        spanSubmit.innerHTML = `<input
                            class="input_button"
                            type="button"
                            value="Opslaan"
                            onclick="saveStream()"
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
                                required
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
                                required
                              />
                            </div>
                            <div id="logo" class="url-container">
                              <div class="edit-label">Logo (kies één optie):</div>
                              <input
                                id="input_logo"
                                class="input_long"
                                type="text"
                                name="logo"
                                value=""
                                placeholder="Logo URL (optioneel)"
                              />
                              <div class="file-upload-container">
                                <label for="file_logo" class="file-upload-label">
                                  📁 Klik om logo te uploaden
                                </label>
                                <input
                                  type="file"
                                  id="file_logo"
                                  accept="image/*"
                                  onchange="handleFileSelect(event)"
                                />
                                <div id="file_info" class="file-info">Geen bestand geselecteerd</div>
                                <div class="file-info">Max 200KB, JPG/PNG/GIF</div>
                              </div>
                            </div>
                        </div>
                     `;
        contentContainer.appendChild(streamForm);
        contentContainer.appendChild(spanSubmit);
      }
      displayStream();
          </script>)RAWSTR";
  searchAndReplace(&script, String("@ip"), ip);
  return script;
}

String getMainScript(String ip)
{

  String script PROGMEM = R"RAWSTR(
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
                      name="newurl_logo_${stream.id}"
                      value="${stream.logo}"
                    />
                    <div class="logo-action-buttons">
                      <!-- Download disabled - conflicts with Audio SSL -->
                      <label class="upload-logo-btn" for="upload_logo_${stream.id}" style="flex: 1;">
                        📁 Upload nieuw logo
                      </label>
                      <input 
                        type="file" 
                        id="upload_logo_${stream.id}" 
                        class="upload-logo-input"
                        accept="image/*"
                        onchange="handleLogoUploadForStream(${stream.id}, '${stream.logo}', this)"
                        style="display: none;"
                      />
                    </div>
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

      async function refreshLogo(streamId, logoUrl) {
        if (!logoUrl || logoUrl.trim() === '') {
          alert('Geen logo URL beschikbaar om te downloaden');
          return;
        }
        
        if (!logoUrl.startsWith('http://') && !logoUrl.startsWith('https://')) {
          alert('Logo URL moet beginnen met http:// of https://');
          return;
        }
        
        if (!confirm('Logo opnieuw downloaden van: ' + logoUrl + '?')) {
          return;
        }
        
        try {
          const response = await fetch('/api/refreshlogo', {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json'
            },
            body: JSON.stringify({ logoUrl: logoUrl })
          });
          
          const result = await response.json();
          
          if (response.ok && result.ok) {
            alert('Logo succesvol gedownload! ✓\n\nHet logo wordt direct gebruikt.');
          } else {
            const errorMsg = result.error || 'Onbekende fout';
            alert('Fout bij downloaden logo:\n' + errorMsg);
          }
        } catch (error) {
          console.error('Error refreshing logo:', error);
          alert('Fout bij downloaden logo:\n' + error.message);
        }
      }

      async function handleLogoUploadForStream(streamId, oldLogoPath, inputElement) {
        const file = inputElement.files[0];
        if (!file) return;
        
        // Validate file type
        if (!file.type.startsWith('image/')) {
          alert('Alleen afbeeldingen zijn toegestaan');
          inputElement.value = '';
          return;
        }
        
        // Validate file size (max 500KB)
        if (file.size > 500 * 1024) {
          alert('Bestand is te groot! Maximum 500KB.');
          inputElement.value = '';
          return;
        }
        
        if (!confirm(`Nieuw logo uploaden: ${file.name}?\n\nDit vervangt het huidige logo.`)) {
          inputElement.value = '';
          return;
        }
        
        try {
          const formData = new FormData();
          formData.append('file', file);
          formData.append('oldLogoPath', oldLogoPath);
          
          const response = await fetch('/api/uploadlogo-replace', {
            method: 'POST',
            body: formData
          });
          
          const result = await response.json();
          
          if (response.ok && result.ok) {
            alert(`Logo succesvol vervangen! ✓\n\nNieuw bestand: ${result.path}\nGrootte: ${result.size} bytes`);
            
            // Update the logo URL input field with the new path
            const logoInput = document.querySelector(`input[name="newurl_logo_${streamId}"]`);
            if (logoInput) {
              logoInput.value = result.path;
            }
          } else {
            const errorMsg = result.error || 'Onbekende fout';
            alert('Fout bij uploaden logo:\n' + errorMsg);
          }
        } catch (error) {
          console.error('Error uploading logo:', error);
          alert('Fout bij uploaden logo:\n' + error.message);
        } finally {
          inputElement.value = '';
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
    </script>)RAWSTR";
  searchAndReplace(&script, String("@ip"), ip);
  return script;
}

String getAlarmScript(String ip)
{
  String script PROGMEM = R"RAWSTR(
    <script>
      const dayNames = ["Zo", "Ma", "Di", "Wo", "Do", "Vr", "Za"];
      const modeOptions = [
        { value: "daily", label: "Dagelijks" },
        { value: "weekdays", label: "Weekdagen" },
        { value: "weekend", label: "Weekend" },
        { value: "weekly", label: "Wekelijks" },
        { value: "custom", label: "Aangepast" },
      ];

      let streams = [];
      let alarms = [];
      let maxAlarmsPerDay = 5;

      function isWeekdayAlarm(alarm) {
        if (alarm.mode === "weekdays" || alarm.mode === "daily") return true;
        return (alarm.dayMask & 0x3e) !== 0;
      }

      function isWeekendAlarm(alarm) {
        if (alarm.mode === "weekend" || alarm.mode === "daily") return true;
        return (alarm.dayMask & 0x41) !== 0;
      }

      function setGroupEnabled(group, enabled) {
        alarms = alarms.map((alarm) => {
          const copy = { ...alarm };
          const matches = group === "weekdays" ? isWeekdayAlarm(copy) : isWeekendAlarm(copy);
          if (matches) {
            copy.enabled = enabled;
          }
          return copy;
        });
        renderAlarmList();
      }

      function modeToMask(mode, currentMask) {
        if (mode === "daily") return 0x7f;
        if (mode === "weekdays") return 0x3e;
        if (mode === "weekend") return 0x41;
        if (mode === "weekly") {
          for (let i = 0; i < 7; i++) {
            if ((currentMask & (1 << i)) !== 0) return 1 << i;
          }
          return 0x02;
        }
        const customMask = currentMask & 0x7f;
        return customMask === 0 ? 0x02 : customMask;
      }

      function createDefaultAlarm() {
        const nextId = alarms.length === 0 ? 1 : Math.max(...alarms.map((a) => a.id)) + 1;
        return {
          id: nextId,
          enabled: true,
          hour: 7,
          minute: 0,
          streamIndex: 0,
          volume: 12,
          mode: "daily",
          dayMask: 0x7f,
          snoozeMinutes: 10,
        };
      }

      function renderAlarmList() {
        const contentContainer = document.getElementById("content-container");
        contentContainer.innerHTML = "";

        const title = document.createElement("h2");
        title.textContent = "Alarmen";
        contentContainer.appendChild(title);

        const subtitle = document.createElement("p");
        subtitle.textContent = `Maximaal ${maxAlarmsPerDay} actieve alarmen per dag. Snooze via presetknop 10.`;
        contentContainer.appendChild(subtitle);

        const header = document.createElement("div");
        header.setAttribute("class", "alarm-header");

        const addButton = document.createElement("button");
        addButton.textContent = "Alarm toevoegen";
        addButton.addEventListener("click", () => {
          alarms.push(createDefaultAlarm());
          renderAlarmList();
        });
        header.appendChild(addButton);

        const saveButton = document.createElement("button");
        saveButton.textContent = "Opslaan";
        saveButton.addEventListener("click", saveAlarms);
        header.appendChild(saveButton);

        const weekdaysOnButton = document.createElement("button");
        weekdaysOnButton.textContent = "Weekdagen aan";
        weekdaysOnButton.addEventListener("click", () => setGroupEnabled("weekdays", true));
        header.appendChild(weekdaysOnButton);

        const weekdaysOffButton = document.createElement("button");
        weekdaysOffButton.textContent = "Weekdagen uit";
        weekdaysOffButton.addEventListener("click", () => setGroupEnabled("weekdays", false));
        header.appendChild(weekdaysOffButton);

        const weekendOnButton = document.createElement("button");
        weekendOnButton.textContent = "Weekend aan";
        weekendOnButton.addEventListener("click", () => setGroupEnabled("weekend", true));
        header.appendChild(weekendOnButton);

        const weekendOffButton = document.createElement("button");
        weekendOffButton.textContent = "Weekend uit";
        weekendOffButton.addEventListener("click", () => setGroupEnabled("weekend", false));
        header.appendChild(weekendOffButton);

        contentContainer.appendChild(header);

        const list = document.createElement("div");
        list.setAttribute("class", "alarm-list");

        alarms.forEach((alarm, idx) => {
          const item = document.createElement("div");
          item.setAttribute("class", "alarm-item");

          const timeValue = `${String(alarm.hour).padStart(2, "0")}:${String(alarm.minute).padStart(2, "0")}`;
          const streamOptions = streams
            .map((s) => `<option value="${s.id}" ${Number(s.id) === Number(alarm.streamIndex) ? "selected" : ""}>${s.name}</option>`)
            .join("");
          const modeSelect = modeOptions
            .map((m) => `<option value="${m.value}" ${m.value === alarm.mode ? "selected" : ""}>${m.label}</option>`)
            .join("");

          item.innerHTML = `
            <div class="alarm-grid">
              <div>
                <label>Actief</label>
                <input data-field="enabled" data-index="${idx}" type="checkbox" ${alarm.enabled ? "checked" : ""} />
              </div>
              <div>
                <label>Tijd</label>
                <input data-field="time" data-index="${idx}" type="time" value="${timeValue}" />
              </div>
              <div>
                <label>Stream</label>
                <select data-field="streamIndex" data-index="${idx}">
                  ${streamOptions}
                </select>
              </div>
              <div>
                <label>Volume</label>
                <input data-field="volume" data-index="${idx}" type="number" min="0" max="30" value="${alarm.volume}" />
              </div>
              <div>
                <label>Herhaling</label>
                <select data-field="mode" data-index="${idx}">
                  ${modeSelect}
                </select>
              </div>
              <div>
                <label>Snooze (min)</label>
                <input data-field="snoozeMinutes" data-index="${idx}" type="number" min="0" max="120" value="${alarm.snoozeMinutes}" />
              </div>
            </div>
            <div class="alarm-days" data-index="${idx}">
              ${dayNames
                .map((name, dayIndex) => {
                  const checked = (alarm.dayMask & (1 << dayIndex)) !== 0 ? "checked" : "";
                  return `<label class="alarm-day"><input type="checkbox" data-field="dayMask" data-day="${dayIndex}" data-index="${idx}" ${checked}/> ${name}</label>`;
                })
                .join("")}
            </div>
            <div class="alarm-actions">
              <button data-action="delete" data-index="${idx}" type="button">Verwijderen</button>
            </div>
          `;

          list.appendChild(item);
        });

        contentContainer.appendChild(list);
        bindAlarmInputs();
      }

      function bindAlarmInputs() {
        document.querySelectorAll("[data-field]").forEach((el) => {
          el.addEventListener("change", (event) => {
            const idx = Number(event.target.dataset.index);
            const field = event.target.dataset.field;
            const alarm = alarms[idx];

            if (!alarm) {
              return;
            }

            if (field === "enabled") {
              alarm.enabled = event.target.checked;
            } else if (field === "time") {
              const parts = event.target.value.split(":");
              alarm.hour = Number(parts[0]);
              alarm.minute = Number(parts[1]);
            } else if (field === "streamIndex") {
              alarm.streamIndex = Number(event.target.value);
            } else if (field === "volume") {
              alarm.volume = Number(event.target.value);
            } else if (field === "mode") {
              alarm.mode = event.target.value;
              alarm.dayMask = modeToMask(alarm.mode, alarm.dayMask);
              renderAlarmList();
              return;
            } else if (field === "snoozeMinutes") {
              alarm.snoozeMinutes = Number(event.target.value);
            } else if (field === "dayMask") {
              const dayIndex = Number(event.target.dataset.day);
              if (event.target.checked) {
                alarm.dayMask |= 1 << dayIndex;
              } else {
                alarm.dayMask &= ~(1 << dayIndex);
              }

              if (alarm.mode === "weekly") {
                alarm.dayMask = 1 << dayIndex;
                renderAlarmList();
                return;
              }
            }
          });
        });

        document.querySelectorAll("[data-action='delete']").forEach((el) => {
          el.addEventListener("click", (event) => {
            const idx = Number(event.target.dataset.index);
            alarms.splice(idx, 1);
            renderAlarmList();
          });
        });
      }

      async function saveAlarms() {
        try {
          const payload = { alarms };
          const response = await fetch("/api/alarms", {
            method: "POST",
            headers: {
              "Content-Type": "application/json",
            },
            body: JSON.stringify(payload),
          });

          if (!response.ok) {
            const errorData = await response.json().catch(() => ({}));
            const message = errorData.message || "Opslaan mislukt";
            alert(message);
            return;
          }

          alert("Alarmen opgeslagen");
          if (typeof window.refreshAlarmStatusBadge === "function") {
            await window.refreshAlarmStatusBadge();
          }
          await loadAlarms();
        } catch (error) {
          console.error(error);
          alert("Er ging iets mis bij opslaan");
        }
      }

      async function loadAlarms() {
        const response = await fetch("http://@ip/api/alarms", {
          method: "GET",
          redirect: "follow",
        });

        if (!response.ok) {
          throw new Error("Kon alarmen niet ophalen");
        }

        const data = await response.json();
        streams = data.streams || [];
        alarms = data.alarms || [];
        maxAlarmsPerDay = data.maxAlarmsPerDay || 5;

        alarms = alarms.map((a) => ({
          id: Number(a.id || 0),
          enabled: Boolean(a.enabled),
          hour: Number(a.hour || 0),
          minute: Number(a.minute || 0),
          streamIndex: Number(a.streamIndex || 0),
          volume: Number(a.volume || 10),
          mode: a.mode || "daily",
          dayMask: Number(a.dayMask || 0x7f),
          snoozeMinutes: Number(a.snoozeMinutes || 10),
        }));

        renderAlarmList();
      }

      loadAlarms().catch((error) => {
        console.error(error);
        alert("Kon alarmgegevens niet laden");
      });
    </script>)RAWSTR";

  searchAndReplace(&script, String("@ip"), ip);
  return script;
}

String getSettingsScript(String ip, int snoozeButtonIndex)
{
  String script PROGMEM = R"SETTINGS(
    <script>
      const contentContainer = document.getElementById("content-container");

      function renderSettings(data) {
        const current = Number(data.snoozeButtonIndex ?? @SNOOZE_BTN@);
        contentContainer.innerHTML = `
          <h2>Instellingen</h2>
          <div class="stream_item" style="max-width:520px;">
            <div class="edit-label">Snooze knop index (PCF8575)</div>
            <input id="snoozeButtonIndex" class="input_short" type="number" min="0" max="15" value="${current}" />
            <div style="margin-top:8px;font-size:13px;">Gebruik een knop die niet als preset gebruikt wordt.</div>
          </div>
          <div style="margin-top:12px;display:flex;gap:10px;">
            <button onclick="saveSettings()">Opslaan</button>
            <button onclick="syncTime()">Tijd synchroniseren</button>
          </div>
        `;
      }

      async function loadSettings() {
        const response = await fetch("http://@ip/api/settings");
        if (!response.ok) {
          throw new Error("Kon instellingen niet laden");
        }
        const data = await response.json();
        renderSettings(data);
      }

      async function saveSettings() {
        const snoozeButtonIndex = Number(document.getElementById("snoozeButtonIndex").value);
        const response = await fetch("/api/settings", {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
          },
          body: JSON.stringify({ snoozeButtonIndex }),
        });

        if (!response.ok) {
          const errorData = await response.json().catch(() => ({}));
          alert(errorData.message || "Opslaan mislukt");
          return;
        }
        alert("Instellingen opgeslagen");
      }

      async function syncTime() {
        const response = await fetch("/api/synctime", { method: "GET" });
        if (!response.ok) {
          alert("Tijd synchroniseren mislukt");
          return;
        }
        alert("Tijd gesynchroniseerd");
      }

      loadSettings().catch((error) => {
        console.error(error);
        alert("Instellingen konden niet geladen worden");
      });
    </script>)SETTINGS";

  searchAndReplace(&script, String("@ip"), ip);
  searchAndReplace(&script, String("@SNOOZE_BTN@"), String(snoozeButtonIndex));
  return script;
}