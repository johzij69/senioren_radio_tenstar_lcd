#include "main.h"
#include "driver/ledc.h" // Include LEDC driver header for PWM functionality

bool debug = true; // Set to true for debug output

int max_volume = MAX_VOLUME; // set default max volume
int last_volume = 10;
int stream_index = -1;
int next_button_state = 0;

String default_url = "https://icecast.omroep.nl/radio1-bb-mp3:443";
String last_url = "";
String prevTime = "";

AudioData audioData;
DisplayData displayData;
MyPreferences myPrefs("myRadio");
UrlManager UrlManagerInstance(myPrefs);
PrioWebServer webServer(UrlManagerInstance, WEB_SERVER_PORT);
PrioRotary rotaryInstance(ROT_CLK_PIN, ROT_DT_PIN);
PrioRfReceiver rfReceiver(RF_RECEIVER_PIN);

/* Buttons */
ezButton rotary_button(ROT_SW_PIN);

// Queues
QueueHandle_t DisplayQueue = xQueueCreate(3, sizeof(DisplayData));
QueueHandle_t AudioQueue = xQueueCreate(3, sizeof(AudioData));

// Maak een instantie van de PrioInputPanel class
PrioInputPanel inputPanel(TOPPANEL_ADDRESS, TOPPANEL_INT_PIN, TOPPANEL_SDA, TOPPANEL_SCL);

// Maak een event group
EventGroupHandle_t taskEvents; // Event group handle for starting order of the tasks

// Task handles
TaskHandle_t displayTaskHandle = NULL;   // Task handle for the TFT task
TaskHandle_t audioTaskHandle = NULL;     // Task handle for the audio task
TaskHandle_t webServerTaskHandle = NULL; // Task handle for the webserver task

PrioDateTime pDateTime(RTC_CLK_PIN, RTC_DAT_PIN, RTC_RST_PIN);

WiFiManager wm;

// Semaphore voor ISR-communicatie
SemaphoreHandle_t powerButtonSemaphore;

// Flags used for power unctions
volatile unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 200; // ms
bool systemLowPower = false;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
    Serial.begin(115200); // Initialize serial communication

    // Zorg er voor dat de RGB led op board uit is en blijft
    // Als je de pin laag zet is er altijd kans dat deze ruis opakt waardoor de led aan gaat!
    strip.begin(); // Initialiseer de strip
    strip.clear(); // Alle pixels op zwart zetten
    strip.show();

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
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
    startDisplayTask();

    // ik wil hier een timestamp loggen
    Serial.println("Waiting for display task to start...");
    char timeBuffer[32];
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.print("Timestamp: ");
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
    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfoe);
    Serial.print("Timestamp eind: ");
    Serial.println(timeBuffer);

    if ((bits & DISPLAY_TASK_STARTED_BIT) == 0)
    {
        Serial.println("Error: Display task start timeout!");
        while (1)
            ; // Blokkeer als display niet start
    }

    displayData.loadingState = false;
    strncpy(displayData.title, "Verbinden met WiFi...", sizeof(displayData.title));
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);

    // power button
    pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
    powerButtonSemaphore = xSemaphoreCreateBinary();
    attachInterrupt(digitalPinToInterrupt(POWER_BUTTON_PIN), handlePowerButtonInterrupt, FALLING);

    WiFi.mode(WIFI_MODE_NULL); // Zorg dat alles uitgezet is
    delay(100);
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // Reduceer WiFi-zendvermogen (minder interferentie)
    WiFi.setTxPower(WIFI_POWER_21dBm); // Experimenteer met lagere waarden
    WiFi.mode(WIFI_STA);

    wm.setConfigPortalTimeout(120); // Langere timeout
    wm.setConnectTimeout(30);       // Verbind timeout
    bool res = wm.autoConnect("prio-radio");
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
        // Start de tijdservice
        Serial.println("Starting time service");
        pDateTime.begin();
        pDateTime.debug = true; // Zet debugmodus aan voor tijdservice

        /* Volume handling */
        last_volume = myPrefs.readValue("volume", DEF_VOLUME);
        if (last_volume > max_volume)
        {
            last_volume = max_volume;
        }

        /* Url handling */
        UrlManagerInstance.begin();

        /* get last used stream*/
        stream_index = myPrefs.getUInt("stream_index", 0);
        if (stream_index >= UrlManagerInstance.streamCount)
        {
            stream_index = 0;
        }

        /* Rotary button */
        pinMode(ROT_CLK_PIN, INPUT);
        pinMode(ROT_DT_PIN, INPUT);
        rotary_button.setDebounceTime(50); // set debounce time to 50 milliseconds
        attachInterrupt(digitalPinToInterrupt(ROT_CLK_PIN), checkVolume, CHANGE);
        attachInterrupt(digitalPinToInterrupt(ROT_DT_PIN), checkVolume, CHANGE);
        rotaryInstance.begin(MIN_VOLUME, max_volume, DEF_VOLUME);
        rotaryInstance.current_value = last_volume;

        inputPanel.begin();                             // Initialiseer de input panel
        inputPanel.setButtonPressedCallback(usePreset); // Stel de callback in

        // Initialiseer de RF-ontvanger
        rfReceiver.begin();
        rfReceiver.setKeyReceivedCallback(usePreset); // Stel de callback-functie in

        // Set ip display data and volume
        strncpy(displayData.ip, WiFi.localIP().toString().c_str(), sizeof(displayData.ip));
        // displayData.syncTime = true; // Reset syncTime flag
        displayData.volume = last_volume;
        audioData.volume = last_volume;

        // Play the last used stream
        usePreset(stream_index);

        Serial.println("Starting webserver task");
        startWebServerTask(); // Start de webserver taak
        Serial.println("Webserver task started");

        Serial.println("Starting audio task");
        startAudioTask();
        Serial.println("Audio task started");
    }
}

/* main loop ;-) */
void loop()
{
    sync_time();

    if (!inStandby)
    {                          
        inputPanel.loop();     // Voer de loop van de input panel uit -> presets buttons and power led
        rfReceiver.loop();     // Voer de loop van de RF-ontvanger uit -> rf remote
        rotaryInstance.loop(); // Voer de loop van de rotary encoder uit -> volume

        if (rotaryInstance.current_value_changed)
        {
            Serial.println("Volume changed to: " + String(rotaryInstance.current_value));
            myPrefs.writeValue("volume", rotaryInstance.current_value);
            displayData.volume = rotaryInstance.current_value;
            audioData.volume = rotaryInstance.current_value;
            last_volume = rotaryInstance.current_value;
            CreateAndSendAudioData(stream_index, rotaryInstance.current_value);
            SendDataToDisplay();
            rotaryInstance.current_value_changed = false;
        }
    }



    if (xSemaphoreTake(powerButtonSemaphore, 0) == pdTRUE)
    {
        systemLowPower = !systemLowPower;

        if (systemLowPower)
        {
            inStandby = true;                // Zet de standby status
            displayData.standbyState = true; // Zet de standby state in display data
            SendDataToDisplay();             // Stuur de display data naar de queue
            Serial.println("⏻ Naar slaapstand...");

            if (webServerTaskHandle != NULL)
            {
                vTaskDelete(webServerTaskHandle);
                webServerTaskHandle = NULL;
                Serial.println("WebServer gestopt.");
            }

            if (audioTaskHandle != NULL)
            {
                vTaskDelete(audioTaskHandle);
                audioTaskHandle = NULL;
                Serial.println("Audio gestopt.");
            }
        }
        else
        {
            inStandby = false;                // Zet de standby status uit
            displayData.standbyState = false; // Zet de standby state uit in display data
            SendDataToDisplay();
            Serial.println("⏻ Systeem hervatten...");

            startWebServerTask(); // Start de webserver taak opnieuw
            startAudioTask();     // Start de audio taak opnieuw
                                  // xTaskCreatePinnedToCore(webServerTask, "WebServer", 8192, NULL, 1, &webServerTaskHandle, 1);
                                  // xTaskCreatePinnedToCore(AudioTask, "Audio", 8192, NULL, 1, &audioTaskHandle, 1);
        }
    }

    vTaskDelay(1 / portTICK_PERIOD_MS); // Adjust the delay as needed (e.g., 10ms)
}

// Interrupt routine just sets a flag when rotation is detected
void IRAM_ATTR checkVolume()
{
    rotaryInstance.rotaryEncoder = true;
}

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

/* Switching radio stream */
void usePreset(int preset)
{
    stream_index = preset;
    CreateAndSendAudioData(stream_index, rotaryInstance.current_value);
    CreateAndSendDisplayData(stream_index);
    /* Save last used stream, do it here so we know stream is working */
    myPrefs.putUInt("stream_index", stream_index);
}

/* Get data and send it to display queue */
void CreateAndSendDisplayData(int streamIndex)
{
    displayData.volume = last_volume;
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
    SendDataToDisplay();
}
void SendDataToDisplay()
{
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
}
/* Get data and send it to audio queue */
void CreateAndSendAudioData(int streamIndex, int last_volume)
{
    AudioData cmd = {};
    cmd.command = CMD_PLAY;
    cmd.volume = last_volume;
    strncpy(cmd.url, UrlManagerInstance.Streams[streamIndex].url.c_str(), sizeof(cmd.url));
    xQueueSend(AudioQueue, &cmd, portMAX_DELAY);
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

void sync_time()
{
    pDateTime.checkSync(); // Controleer of synchronisatie nodig is at the moment ones every hour
    strncpy(displayData.currenTime, pDateTime.getTime(), sizeof(displayData.currenTime));
    if (prevTime != displayData.currenTime)
    {
        prevTime = displayData.currenTime;
        xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
    }
}

void startDisplayTask()
{
    if (displayTaskHandle == NULL)
    {
        BaseType_t result = xTaskCreate(
            DisplayTask,
            "DisplayTask",
            8192,
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
            8192,                  // Stack size in words
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
            8192,               // Stack size in words
            (void *)AudioQueue, // Task parameter
            1,                  // Priority of the task
            &audioTaskHandle);  // Task handle
    }
}