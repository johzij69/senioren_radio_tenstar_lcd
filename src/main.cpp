#include "main.h"
#include "AlarmManager.h"
#include "UrlManager.h"
#include "driver/ledc.h" // Include LEDC driver header for PWM functionality
#include "PrioRotaryMenu.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

bool debug = false; // Set to true for debug output
uint8_t alarmSnoozeButtonIndex = 10;

// int max_volume = MAX_VOLUME; // set default max volume
// int last_volume = 10;
int stream_index = -1;
int next_button_state = 0;

String default_url = "https://icecast.omroep.nl/radio1-bb-mp3:443";
String last_url = "";
String prevTime = "";

AudioData audioData;
DisplayData displayData;
MyPreferences myPrefs("myRadio");
UrlManager UrlManagerInstance(myPrefs);
AlarmManager alarmManager(myPrefs);
PrioWebServer webServer(UrlManagerInstance, alarmManager, myPrefs, WEB_SERVER_PORT);
PrioRfReceiver rfReceiver(RF_RECEIVER_PIN);


// Queues
QueueHandle_t DisplayQueue = xQueueCreate(3, sizeof(DisplayData));
QueueHandle_t AudioQueue = xQueueCreate(3, sizeof(AudioData));
QueueHandle_t AudioEventQueue = xQueueCreate(4, sizeof(AudioEvent));
QueueHandle_t DlnaCommandQueue = xQueueCreate(4, sizeof(DlnaCommand));
QueueHandle_t DlnaEventQueue = xQueueCreate(4, sizeof(DlnaEvent));

// Maak een instantie van de PrioInputPanel class
PrioInputPanel inputPanel(TOPPANEL_ADDRESS, TOPPANEL_INT_PIN, TOPPANEL_SDA, TOPPANEL_SCL);

// Maak een event group
EventGroupHandle_t taskEvents; // Event group handle for starting order of the tasks

// Task handles
TaskHandle_t displayTaskHandle = NULL;   // Task handle for the TFT task
TaskHandle_t audioTaskHandle = NULL;     // Task handle for the audio task
TaskHandle_t webServerTaskHandle = NULL; // Task handle for the webserver task
TaskHandle_t dlnaTaskHandle = NULL;      // Task handle for the DLNA discovery/browse task



WiFiManager wm;

// Semaphore voor ISR-communicatie
SemaphoreHandle_t powerButtonSemaphore;
SemaphoreHandle_t sleepButtonSemaphore;

// Flags used for power unctions
volatile unsigned long lastInterruptTime = 0;
volatile unsigned long lastSleepInterruptTime = 0;
const unsigned long debounceDelay = 200; // ms
bool systemLowPower = false;

static bool sleepSessionActive = false;
static unsigned long sleepSessionDeadlineMs = 0;

static void getDisplayStatusText(char *out, size_t outSize)
{
    if (sleepSessionActive)
    {
        long remainingMs = (long)(sleepSessionDeadlineMs - millis());
        if (remainingMs < 0)
        {
            remainingMs = 0;
        }

        // Toon alleen hele minuten om schermflikkering door seconde-updates te beperken.
        unsigned long remainingMinutes = ((unsigned long)remainingMs + 59999UL) / 60000UL;
        snprintf(out, outSize, "Sleep: %lu min", remainingMinutes);
        return;
    }

    alarmManager.getDisplayStatusText(time(nullptr), out, outSize);
}

static uint16_t clampSleepMinutes(int minutes)
{
    if (minutes < SLEEP_MIN_MINUTES)
    {
        return SLEEP_DEFAULT_MINUTES;
    }
    if (minutes > SLEEP_MAX_MINUTES)
    {
        return SLEEP_MAX_MINUTES;
    }
    return (uint16_t)minutes;
}

static uint16_t getConfiguredSleepMinutes()
{
    return clampSleepMinutes((int)myPrefs.getUInt("sleep_minutes", SLEEP_DEFAULT_MINUTES));
}

static void enterStandbyMode()
{
    sleepSessionActive = false;
    systemLowPower = true;
    inStandby = true;
    displayData.standbyState = true;
    strncpy(displayData.currenTime, pDateTime.getTime(), sizeof(displayData.currenTime));
    displayData.currenTime[sizeof(displayData.currenTime) - 1] = '\0';
    strncpy(displayData.currenDate, pDateTime.getDayDate(), sizeof(displayData.currenDate));
    displayData.currenDate[sizeof(displayData.currenDate) - 1] = '\0';
    SendDataToDisplay();
    stopAudio();
    pauseAudioTask();
}

static void leaveStandbyModeAndResumePlayback()
{
    systemLowPower = false;
    inStandby = false;
    displayData.standbyState = false;
    CreateAndSendDisplayData(stream_index);
    resumeAudioTask();
    playStream(stream_index);
}

static void startSleepSessionFromStandby()
{
    if (!inStandby)
    {
        return;
    }

    if (UrlManagerInstance.streamCount == 0)
    {
        Serial.println("Sleep niet gestart: geen streams beschikbaar");
        return;
    }

    uint16_t sleepMinutes = getConfiguredSleepMinutes();
    int rememberedStreamIndex = (int)myPrefs.getUInt("stream_index", 0);
    if (rememberedStreamIndex < 0 || rememberedStreamIndex >= (int)UrlManagerInstance.streamCount)
    {
        rememberedStreamIndex = 0;
    }

    systemLowPower = false;
    inStandby = false;
    displayData.standbyState = false;
    resumeAudioTask();
    playStream(rememberedStreamIndex);

    sleepSessionActive = true;
    sleepSessionDeadlineMs = millis() + ((unsigned long)sleepMinutes * 60UL * 1000UL);
    Serial.println("Sleep gestart voor " + String(sleepMinutes) + " minuten");
}

static void handleSleepSessionTimeout()
{
    if (!sleepSessionActive)
    {
        return;
    }

    if ((long)(millis() - sleepSessionDeadlineMs) < 0)
    {
        return;
    }

    Serial.println("Sleep timer klaar, terug naar standby");
    enterStandbyMode();
}

//Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
    Serial.begin(115200); // Initialize serial communication

  
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW); // Zet de ingebouwde LED uit bij opstarten

    // Moet vóór elke TLS-verbinding gebeuren
    enableTlsPsramAllocator();

    // Om er zeker van te zijn dat de i2c bus voor de licht sensor en Top panel actief is
    Wire.begin(TOPPANEL_SDA, TOPPANEL_SCL); // Initialize I2C bus for the top panel and light sensor

    if (debug)
    {
        Serial.println("Starting Prio Radio...");
        delay(10000);            // Wait for serial to initialize
        wm.setDebugOutput(true); // Debug-logging aan
        Serial.println("Debug mode is ON");
    }

    // Initialiseer het event group
    taskEvents = xEventGroupCreate();
    if (taskEvents == NULL)
    {
        Serial.println("FOUT: Event group kon niet worden aangemaakt!");
    }
    else
    {
        Serial.println("Event group aangemaakt.");
    }
    Serial.println("Starting tasks");

    displayData.loadingState = true; // Set loading state to true initially
    refreshAlarmDisplayState(); // definitieve status volgt na alarmManager.begin()
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
    startDisplayTask();

    // ik wil hier een timestamp loggen
    Serial.println("Waiting for display task to start...");
    char timeBuffer[32];
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
    Serial.print("Timestamp for start display task check: ");
    Serial.println(timeBuffer);
    // Wacht op het event dat de display task is gestart
    EventBits_t bits = xEventGroupWaitBits(
        taskEvents,               // Event group handle
        DISPLAY_TASK_STARTED_BIT, // Bits om op te wachten
        pdFALSE,                  // CLEAR de bits na het wachten
        pdTRUE,                   // Wacht op ALLE opgegeven bits
        pdMS_TO_TICKS(55000)      // Timeout van 5000 ms
    );

    time_t nowe = time(nullptr);
    struct tm timeinfoe;
    localtime_r(&nowe, &timeinfoe);
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfoe);
    Serial.print("Timestamp start display task check eind: ");
    Serial.println(timeBuffer);

    if ((bits & DISPLAY_TASK_STARTED_BIT) == 0)
    {
        Serial.println("Error: Display task start timeout!");
        while (1)
            ; // Blokkeer als display niet start
    }

  Serial.println("Continuing with setup wifi");
//    const char* ssid = "WiFi-2.4-E770";
//    const char* password = "wub4yhd65nwb7";

   // WiFi.mode(WIFI_MODE_NULL); // Zorg dat alles uitgezet is
    // delay(100);
    // WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // // // Reduceer WiFi-zendvermogen (minder interferentie)
    //  WiFi.setTxPower(WIFI_POWER_21dBm); // Experimenteer met lagere waarden
    // WiFi.mode(WIFI_STA);

    wm.setConfigPortalTimeout(120); // Langere timeout
    wm.setConnectTimeout(30);       // Verbind timeout
    Serial.println("Starting WiFi autoConnect...");

    // WiFi.mode(WIFI_STA);
    // WiFi.begin(ssid, password);
    // Serial.println("\nConnecting to WiFi Network ..");

    // while(WiFi.status() != WL_CONNECTED){
    //     Serial.print(".");
    //     delay(100);
    // }

    Serial.println("\nConnected to the WiFi network");
    Serial.print("Local ESP32 IP: ");
    Serial.println(WiFi.localIP());

    displayData.loadingState = false;
    strncpy(displayData.title, "Verbinden met WiFi...", sizeof(displayData.title));
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);

    // power button
    pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
    powerButtonSemaphore = xSemaphoreCreateBinary();
    attachInterrupt(digitalPinToInterrupt(POWER_BUTTON_PIN), handlePowerButtonInterrupt, FALLING);

    // sleep button
    pinMode(SLEEP_BUTTON_PIN, INPUT_PULLUP);
    sleepButtonSemaphore = xSemaphoreCreateBinary();
    attachInterrupt(digitalPinToInterrupt(SLEEP_BUTTON_PIN), handleSleepButtonInterrupt, FALLING);

   Serial.println("Power button initialized.");
    bool res = wm.autoConnect("prio-radio");
    //bool res = (WiFi.status() == WL_CONNECTED);
    Serial.println("WiFi autoConnect result: " + String(res));
    if (!res)
    {
        Serial.println("Failed to connect");
        Serial.println("[ERROR] Verbinden mislukt. Config portal gestart.");
        // Maak een functie om de foutmelding weer te geven op het tft scherm
        strncpy(displayData.title, "Verbinden met Failed...", sizeof(displayData.title));
        xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
        return;
    }
    else
    {

        Serial.println("[OK] Verbonden met WiFi");
        Serial.println("SSID: " + WiFi.SSID());
        Serial.println("IP: " + WiFi.localIP().toString());
        Serial.print("DNS address: ");
        Serial.println(WiFi.dnsIP());
        Serial.print("Gateway address: ");
        Serial.println(WiFi.gatewayIP());

        myPrefs.begin();
        alarmManager.begin();
        refreshAlarmDisplayState(); // pas nu zijn de opgeslagen alarmen bekend
        alarmSnoozeButtonIndex = (uint8_t)myPrefs.getUInt("snooze_btn_idx", 10);
        if (alarmSnoozeButtonIndex > 15)
        {
            alarmSnoozeButtonIndex = 10;
        }
        // Start de tijdservice
        Serial.println("Starting time service");
        pDateTime.begin();
        pDateTime.debug = true; // Zet debugmodus aan voor tijdservice




        
        /* Url handling */
        UrlManagerInstance.begin();

        /* get last used stream*/
        stream_index = myPrefs.getUInt("stream_index", 0);
        if (stream_index >= UrlManagerInstance.streamCount)
        {
            stream_index = 0;
        }

        // /* Rotary button */
        // pinMode(ROT_CLK_PIN, INPUT);
        // pinMode(ROT_DT_PIN, INPUT);
        // rotary_button.setDebounceTime(50); // set debounce time to 50 milliseconds
        // attachInterrupt(digitalPinToInterrupt(ROT_CLK_PIN), checkVolume, CHANGE);
        // attachInterrupt(digitalPinToInterrupt(ROT_DT_PIN), checkVolume, CHANGE);
 //       rotaryInstance.begin(MIN_VOLUME, max_volume, DEF_VOLUME);
   //     rotaryInstance.current_value = last_volume;

        inputPanel.begin();                                       // Initialiseer de input panel
        inputPanel.setButtonPressedCallback(handleInputPanelButton); // Stel de callback in

        // Initialiseer de RF-ontvanger
        rfReceiver.begin();
        rfReceiver.setKeyReceivedCallback(playStream); // Stel de callback-functie in

        // Set ip display data and volume
        strncpy(displayData.ip, WiFi.localIP().toString().c_str(), sizeof(displayData.ip));
        // displayData.syncTime = true; // Reset syncTime flag
//        displayData.volume = last_volume;
//        audioData.volume = last_volume;


//    last_volume_for_menu = last_volume; // Sync initial volume

        Serial.println("Starting webserver task");
        startWebServerTask(); // Start de webserver taak
        Serial.println("Webserver task started");

        Serial0.println("Starting audio task");
        startAudioTask();
        Serial.println("Audio task started");

        Serial.println("Starting DLNA task");
        startDlnaTask();
        Serial.println("DLNA task started");

        // Play the last used stream
        playStream(stream_index);



    }


    
}

/* main loop ;-) */
void loop()
{
    // checkSync() zelf gooit alleen elk uur (zie PrioDateTime::_syncInterval) een
    // echte NTP-sync eruit; deze aanroep is dus goedkoop om elke loop te doen en
    // is - samen met de onvoorwaardelijke sync in PrioDateTime::begin() - de enige
    // plek die de RTC-tijd periodiek corrigeert (drift, gemiste zomertijdwissel).
    pDateTime.checkSync();
    updateClockDisplay();
    checkAndRunAlarms();

    if (!inStandby)
    {
        inputPanel.loop();     // Voer de loop van de input panel uit -> presets buttons and power led
        rfReceiver.loop();     // Voer de loop van de RF-ontvanger uit -> rf remote
        EventBits_t uxBits = xEventGroupWaitBits(taskEvents, MENU_CLOSED_REQUEST_DATA_BIT, pdTRUE, pdFALSE, 0);
        if (uxBits & MENU_CLOSED_REQUEST_DATA_BIT) {
            CreateAndSendDisplayData(stream_index); // Stuur verse data naar de queue
        }
    }

    if (xSemaphoreTake(powerButtonSemaphore, 0) == pdTRUE)
    {
        systemLowPower = !systemLowPower;

        if (systemLowPower)
        {
            Serial.println("Naar standby...");
            enterStandbyMode();
        }
        else
        {
            Serial.println("Systeem hervatten...");
            leaveStandbyModeAndResumePlayback();
        }
    }

    if (xSemaphoreTake(sleepButtonSemaphore, 0) == pdTRUE)
    {
        if (inStandby)
        {
            startSleepSessionFromStandby();
        }
        else
        {
            Serial.println("Sleep-knop genegeerd: radio is niet in standby");
        }
    }

    handleSleepSessionTimeout();


    vTaskDelay(1 / portTICK_PERIOD_MS); // Adjust the delay as needed (e.g., 10ms)
}

void handleInputPanelButton(int buttonIndex)
{
    if (buttonIndex == alarmSnoozeButtonIndex)
    {
        snoozeActiveAlarm();
        return;
    }

    if (buttonIndex >= 0 && buttonIndex < (int)UrlManagerInstance.streamCount)
    {
        //playStream(buttonIndex);
        // We mounted the button panel the wrong way, so this is the software fix. ;-)
        playStream((UrlManagerInstance.streamCount-1) - buttonIndex);
    }
    else
    {
        Serial.println("Onbekende button index: " + String(buttonIndex));
    }
}

// The on-screen clock was only ever refreshed as a side effect of
// Task_Display.cpp receiving a DisplayQueue message for something else
// (station/title/streamtitle/volume/alarm change) - see the strncpy right
// before showTime() there. A station that doesn't send frequent ICY
// StreamTitle updates (or standby, where nothing else touches the queue at
// all) left the displayed clock frozen at whatever time that last incidental
// update happened, growing further "behind" the longer nothing else changed.
// This gives it its own independent tick, throttled to once a second like
// checkAndRunAlarms() below, and only actually sends when the minute changes
// so it doesn't spam the queue every second for nothing.
//
// Deliberately does NOT call pDateTime.getTime() here: pDateTime bit-bangs the
// RTC over shared GPIO pins and writes into one shared internal buffer for
// every get*() call, and Task_Display.cpp already calls pDateTime.getTime()
// on every DisplayQueue message it receives (DisplayTask). Calling it again
// from this task raced with that read/write - most reproducibly right at the
// instant the minute changes, since that's exactly when both tasks notice it
// at once - and tore the buffer content (missing/garbled last character,
// worst on the standby screen). time()/localtime_r() are backed by the
// ESP-IDF's own NTP-synced system clock, not the RTC chip, so they're safe to
// poll from any task; this only uses them to detect "a minute passed" and
// then pokes the queue so DisplayTask's own (already-safe, single-task) RTC
// read fires and refreshes the screen.
void updateClockDisplay()
{
    static unsigned long lastCheck = 0;
    static int lastMinute = -1;

    unsigned long nowMs = millis();
    if (nowMs - lastCheck < 1000)
    {
        return;
    }
    lastCheck = nowMs;

    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_min == lastMinute) return;
    lastMinute = timeinfo.tm_min;

    SendDataToDisplay();
}

void checkAndRunAlarms()
{
    static unsigned long lastAlarmCheck = 0;
    unsigned long nowMs = millis();
    if (nowMs - lastAlarmCheck < 1000)
    {
        return;
    }
    lastAlarmCheck = nowMs;

    time_t now = time(nullptr);
    AlarmManager::AlarmEntry alarm;
    bool fromSnooze = false;
    if (alarmManager.poll(now, alarm, fromSnooze))
    {
        triggerAlarmPlayback(alarm, fromSnooze);
        return;
    }

    // Het getoonde tijdstip verschuift zodra een alarm voorbij is, of de sleep-timer aftelt.
    char statusText[sizeof(displayData.alarmState)];
    getDisplayStatusText(statusText, sizeof(statusText));
    if (strncmp(statusText, displayData.alarmState, sizeof(statusText)) != 0)
    {
        strncpy(displayData.alarmState, statusText, sizeof(displayData.alarmState));
        displayData.alarmState[sizeof(displayData.alarmState) - 1] = '\0';
        SendDataToDisplay();
    }
}

void triggerAlarmPlayback(const AlarmManager::AlarmEntry &alarm, bool fromSnooze)
{
    if (alarm.streamIndex >= UrlManagerInstance.streamCount)
    {
        Serial.println("Alarm genegeerd: ongeldige streamIndex");
        return;
    }

        refreshAlarmDisplayState();
    strncpy(displayData.alarmState, fromSnooze ? "Snooze actief" : "Alarm actief", sizeof(displayData.alarmState));

    sleepSessionActive = false;
    systemLowPower = false;
    inStandby = false;
    displayData.standbyState = false;
    resumeAudioTask();

    int alarmVolume = alarm.volume;
    // if (alarmVolume > max_volume)
    // {
    //     alarmVolume = max_volume;
    // }

    stream_index = alarm.streamIndex;
 //   last_volume = alarmVolume;
 //   rotaryInstance.current_value = alarmVolume;
    displayData.volume = alarmVolume;
    audioData.volume = alarmVolume;
    myPrefs.writeValue("volume", alarmVolume);
//todo pas alarm volume door aan audiocontrol
    playAudio(UrlManagerInstance.Streams[stream_index].url.c_str());
    CreateAndSendDisplayData(stream_index);
}

void snoozeActiveAlarm()
{
    time_t snoozeUntil = 0;
    if (!alarmManager.snoozeCurrentAlarm(time(nullptr), snoozeUntil))
    {
        Serial.println("Geen actief alarm om te snoozen.");
        return;
    }

    stopAudio();
    refreshAlarmDisplayState(true);
    Serial.println("Alarm gesnoozed tot epoch: " + String((uint32_t)snoozeUntil));
}

void refreshAlarmDisplayState(bool sendToDisplay)
{
    getDisplayStatusText(displayData.alarmState, sizeof(displayData.alarmState));

    if (sendToDisplay)
    {
        SendDataToDisplay();
    }
}

// // Interrupt routine just sets a flag when rotation is detected
// void IRAM_ATTR checkVolume()
// {
//     rotaryInstance.rotaryEncoder = true;
// }

void IRAM_ATTR handlePowerButtonInterrupt()
{
    unsigned long currentTime = millis();
    if (currentTime - lastInterruptTime > debounceDelay)
    {
        lastInterruptTime = currentTime;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(powerButtonSemaphore, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR();
        }
    }
}

void IRAM_ATTR handleSleepButtonInterrupt()
{
    unsigned long currentTime = millis();
    if (currentTime - lastSleepInterruptTime > debounceDelay)
    {
        lastSleepInterruptTime = currentTime;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(sleepButtonSemaphore, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR();
        }
    }
}

/* Get data and send it to display queue */
void CreateAndSendDisplayData(int streamIndex)
{
//    displayData.volume = last_volume;
    // displayData.syncTime = true;      // Reset syncTime flag
    displayData.loadingState = false;
    displayData.standbyState = inStandby; // Set standby state
    strncpy(displayData.ip, WiFi.localIP().toString().c_str(), sizeof(displayData.ip));
    strncpy(displayData.title, UrlManagerInstance.Streams[streamIndex].name.c_str(), sizeof(displayData.title));
    strncpy(displayData.logo, UrlManagerInstance.Streams[streamIndex].logo.c_str(), sizeof(displayData.logo));
    strncpy(displayData.bitrate, "Loading...", sizeof(displayData.bitrate));
    strncpy(displayData.station, "Loading...", sizeof(displayData.station));
    strncpy(displayData.icyurl, "Loading...", sizeof(displayData.icyurl));
    strncpy(displayData.lasthost, "Loading...", sizeof(displayData.lasthost));
    strncpy(displayData.streamtitle, "", sizeof(displayData.streamtitle));
    strncpy(displayData.currenTime, pDateTime.getTime(), sizeof(displayData.currenTime));
    strncpy(displayData.currenDate, pDateTime.getDayDate(), sizeof(displayData.currenDate));
    SendDataToDisplay();
}
void SendDataToDisplay()
{
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
}

// PrioDlnaBrowser hands a chosen track here instead of calling playAudio()
// directly, so its DIDL-Lite metadata reaches the screen the same way preset
// info does via CreateAndSendDisplayData() above. "?" is PrioDlnaClient's
// sentinel for "field not present in this item" - treated as empty here.
void playDlnaTrack(const char* url, const char* title, const char* artist,
                    const char* album, const char* albumArtURI)
{
    auto present = [](const char* s) { return s && s[0] != '\0' && strcmp(s, "?") != 0; };

    playAudio(url, true);

    displayData.loadingState = false;
    displayData.standbyState = inStandby;
    strncpy(displayData.ip, WiFi.localIP().toString().c_str(), sizeof(displayData.ip));
    strncpy(displayData.title, present(title) ? title : "DLNA", sizeof(displayData.title));

    String subtitle;
    if (present(artist)) subtitle = artist;
    if (present(album)) { if (subtitle.length()) subtitle += " - "; subtitle += album; }
    strncpy(displayData.station, subtitle.c_str(), sizeof(displayData.station));

    strncpy(displayData.logo, present(albumArtURI) ? albumArtURI : "", sizeof(displayData.logo));
    strncpy(displayData.bitrate, "", sizeof(displayData.bitrate));
    strncpy(displayData.icyurl, "", sizeof(displayData.icyurl));
    strncpy(displayData.lasthost, "", sizeof(displayData.lasthost));
    strncpy(displayData.streamtitle, "", sizeof(displayData.streamtitle));
    strncpy(displayData.currenTime, pDateTime.getTime(), sizeof(displayData.currenTime));
    SendDataToDisplay();
}

// PrioDlnaBrowser::CancelledCallback: the browser stopped playback on open
// (see PrioDlnaBrowser::start()) and closed again without a track chosen -
// pick up where we left off. stream_index isn't touched by playDlnaTrack(),
// so it still reflects the last favorite played even if DLNA tracks were
// played in between.
void resumePreviousStream()
{
    if (UrlManagerInstance.streamCount == 0) return; // no favorites configured
    int idx = (stream_index >= 0 && stream_index < (int)UrlManagerInstance.streamCount) ? stream_index : 0;
    playStream(idx);
}

/* Audio events */
void audio_showstation(const char *info)
{
    strncpy(displayData.station, info, sizeof(displayData.station));
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
}
void audio_showstreamtitle(const char *info)
{
    strncpy(displayData.streamtitle, info, sizeof(displayData.streamtitle));
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
}
void audio_bitrate(const char *info)
{
    strncpy(displayData.bitrate, info, sizeof(displayData.bitrate));
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
}

void audio_info(const char *info)
{
    Serial.printf("[AUDIO_LIB] %s\n", info);
}



void startDisplayTask()
{
    if (displayTaskHandle == NULL)
    {
        BaseType_t result = xTaskCreate(
            DisplayTask,
            "DisplayTask",
            5120,
            (void *)DisplayQueue,
            5,
            &displayTaskHandle);

        if (result != pdPASS)
        {
            Serial.println("FOUT: DisplayTask kon niet worden gestart!");
        }
        else
        {
            Serial.println("DisplayTask is gestart.");
        }
    }
}
void startWebServerTask()
{
    if (webServerTaskHandle == NULL)
    {
        xTaskCreate(
            webServerTask,         // Task function
            "webServerTask",       // Name of the task
            5120,                  // Stack size in words
            (void *)&webServer,    // Task parameter
            5,                     // Priority of the task
            &webServerTaskHandle); // Task handle
    }
}
void startAudioTask()
{
    if (audioTaskHandle == NULL)
    {
        xTaskCreate(
            AudioTask,          // Task function
            "AudioTask",        // Name of the task
            16384,               // Stack size in words
            (void *)AudioQueue, // Task parameter
            6,                  // Priority of the task (HIGHER than DisplayTask)
            &audioTaskHandle);  // Task handle
    }
}

void startDlnaTask()
{
    if (dlnaTaskHandle == NULL)
    {
        xTaskCreate(
            DlnaTask,                  // Task function
            "DlnaTask",                // Name of the task
            8192,                      // Stack size in words
            (void *)DlnaCommandQueue,  // Task parameter
            3,                         // Priority (background, lower than Display/Audio)
            &dlnaTaskHandle);          // Task handle
    }
}

void pauseAudioTask()
{
    if (audioTaskHandle != NULL)
    {
        vTaskSuspend(audioTaskHandle);
        Serial.println("Audio task paused.");
    }
}

void resumeAudioTask()
{
    if (audioTaskHandle == NULL)
    {
        BaseType_t result = xTaskCreate(
            AudioTask,
            "AudioTask",
            16384,
            (void *)AudioQueue,
            6,                  // Priority (HIGHER than DisplayTask)
            &audioTaskHandle);

        if (result != pdPASS)
        {
            Serial.println("FOUT: AudioTask kon niet worden gestart!");
        }
        else
        {
            Serial.println("AudioTask is gestart.");
        }
    }
    else
    {
        vTaskResume(audioTaskHandle);
        Serial.println("Audio task resumed.");
    }
}

/* Switching radio stream */
void playStream(int preset)
{
    alarmManager.stopRinging();
    refreshAlarmDisplayState();
    Serial.println("Switching to stream: " + String(preset));
    stream_index = preset;
//    playAudio(UrlManagerInstance.Streams[stream_index].url.c_str(), rotaryInstance.current_value);
    playAudio(UrlManagerInstance.Streams[stream_index].url.c_str());
    CreateAndSendDisplayData(stream_index);
    /* Save last used stream, do it here so we know stream is working */
    myPrefs.putUInt("stream_index", stream_index);
}


