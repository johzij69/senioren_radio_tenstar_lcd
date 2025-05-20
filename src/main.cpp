#include "main.h"
#include "driver/ledc.h" // Include LEDC driver header for PWM functionality

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

// Semaphore voor ISR-communicatie
SemaphoreHandle_t powerButtonSemaphore;

// Flags used for power unctions
volatile unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 200; // ms
bool systemLowPower = false;

void setup()
{
    Serial.begin(115200); // Initialize serial communication
    WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP
    WiFiManager wm;
    bool res;
    res = wm.autoConnect("prio-radio");
    if (!res)
    {
        Serial.println("Failed to connect");
        return;
    }
    else
    {
        Serial.println("WiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("DNS address: ");
        Serial.println(WiFi.dnsIP());
        Serial.print("Gateway address: ");
        Serial.println(WiFi.gatewayIP());

        // power button
        pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
        powerButtonSemaphore = xSemaphoreCreateBinary();
        attachInterrupt(digitalPinToInterrupt(POWER_BUTTON_PIN), handlePowerButtonInterrupt, FALLING);

        myPrefs.begin();
        // Start de tijdservice
        Serial.println("Starting time service");
        pDateTime.begin();

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
        displayData.syncTime = true; // Reset syncTime flag
        displayData.volume = last_volume;
        audioData.volume = last_volume;

        // Play the last used stream
        usePreset(stream_index);

        // Initialiseer het event group
        taskEvents = xEventGroupCreate();

        Serial.println("Starting tasks");
        //   Create the FreeRTOS task for the Display
        xTaskCreate(
            DisplayTask,          // Task function
            "DisplayTask",        // Name of the task
            8192,                 // Stack size in words
            (void *)DisplayQueue, // Task parameter
            5,                    // Priority of the task
            &displayTaskHandle);  // Task handle

        xTaskCreatePinnedToCore(
            webServerTask,      // Task function
            "webServerTask",    // Name of the task
            4096,               // Stack size in words
            (void *)&webServer, // Task parameter
            5,                  // Priority of the task
            &webServerTaskHandle,
            1); // Task handle

        // Create the FreeRTOS task for the AudioTask
        xTaskCreatePinnedToCore(
            AudioTask,            // Task function
            "AudioTask",          // Name of the task
            8192,                 // Stack size in words
            (void *)AudioQueue,   // Task parameter
            1,                    // Priority of the task
            &audioTaskHandle, 1); // Task handle
    }
}

/* main loop ;-) */
void loop()
{

    //    ledc_set_duty(LEDC_LOW_SPEED_MODE, pwmChannel, 125);
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
        xQueueSend(AudioQueue, &audioData, portMAX_DELAY);
        xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
        rotaryInstance.current_value_changed = false;
    }

    sync_time();

    if (xSemaphoreTake(powerButtonSemaphore, 0) == pdTRUE)
    {
        systemLowPower = !systemLowPower;

        if (systemLowPower)
        {
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
            Serial.println("⏻ Systeem hervatten...");

            xTaskCreatePinnedToCore(webServerTask, "WebServer", 8192, NULL, 1, &webServerTaskHandle, 1);
            xTaskCreatePinnedToCore(AudioTask, "Audio", 8192, NULL, 1, &audioTaskHandle, 1);
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
    strncpy(displayData.ip, WiFi.localIP().toString().c_str(), sizeof(displayData.ip));
    strncpy(displayData.title, UrlManagerInstance.Streams[streamIndex].name.c_str(), sizeof(displayData.title));
    strncpy(displayData.logo, UrlManagerInstance.Streams[streamIndex].logo.c_str(), sizeof(displayData.logo));
    strncpy(displayData.bitrate, "Loading...", sizeof(displayData.bitrate));
    strncpy(displayData.station, "Loading...", sizeof(displayData.station));
    strncpy(displayData.icyurl, "Loading...", sizeof(displayData.icyurl));
    strncpy(displayData.lasthost, "Loading...", sizeof(displayData.lasthost));
    strncpy(displayData.streamtitle, "Wachten op titel...", sizeof(displayData.streamtitle));
    strncpy(displayData.currenTime, pDateTime.getTime(), sizeof(displayData.currenTime));
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


    // audioData.volume = last_volume;
    // strncpy(audioData.url, UrlManagerInstance.Streams[streamIndex].url.c_str(), sizeof(audioData.url));
    // xQueueSend(AudioQueue, &audioData, portMAX_DELAY);
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
