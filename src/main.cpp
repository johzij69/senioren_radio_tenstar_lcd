// vTask explained -> https://www.youtube.com/watch?v=ywbq1qR-fY0

#include "main.h"
#include <WiFiManager.h>
// #include "Audio.h"
#include "PrioRotary.h"
#include <ezButton.h> // the library to use for SW pin
#include "MyPreferences.h"
#include "PrioWebserver.h"
#include "UrlManager.h"
#include "PrioTft.h"
// #include <TFT_eSPI.h>
// #include "PrioBar.h"
// #include "StreamLogo.h"
// #include "PrioScrollText.h"
#include "Free_Fonts.h"
// #include "TFTScroller.h"
// #include "smallFont.h"  // bar
// #include "middleFont.h" // bar
// #include <SPIFFS.h> // Include the SPIFFS library
// #include <PRIO_GT911.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "task_webServer.h"
#include "Task_Audio.h"
#include "Task_Display.h"

#define WM_ERASE_NVS

// Rotary Enocder used for Volume control
#define ROT_CLK_PIN 17  // ESP32 pin GPIO17 connected to the rotary encoder's CLK pin
#define ROT_DT_PIN 16   // ESP32 pin GPIO16 connected to the rotary encoder's DT pin
#define ROT_SW_PIN 15   // ESP32 pin GPIO15 connected to the rotary encoder's SW pin
#define DIRECTION_CW 0  // clockwise direction
#define DIRECTION_CCW 1 // counter-clockwise direction

/* Next Button */
#define NEXT_BUTTON_PIN 21 // ESP32 pin GPIO33, which connected to button

#define VOLUME_STEPS 30
#define MIN_VOLUME 0
#define DEF_VOLUME 15

// /* touch */
// #define TOUCH_SDA 48
// #define TOUCH_SCL 47
// #define TOUCH_WIDTH 480
// #define TOUCH_HEIGHT 320

int max_volume = 30; // set default max volume
int last_volume = 10;
int stream_index = -1;
int next_button_state = 0;

const char *current_url;

bool next_button_isreleased = true;
bool streamSwitched = false;

String default_url = "https://icecast.omroep.nl/radio1-bb-mp3:443";
String last_url;

PrioRotary rotaryInstance(ROT_CLK_PIN, ROT_DT_PIN);
ezButton rotary_button(ROT_SW_PIN);
ezButton next_button(NEXT_BUTTON_PIN);
MyPreferences myPrefs("myRadio");
UrlManager UrlManagerInstance(myPrefs);
PrioWebServer webServer(UrlManagerInstance, 80);



DisplayData displayData;
AudioData audioData;

// Queues
QueueHandle_t logoQueue = xQueueCreate(3, sizeof(int));
QueueHandle_t streamQueue = xQueueCreate(3, sizeof(int));
QueueHandle_t volumeQueue = xQueueCreate(3, sizeof(int));
QueueHandle_t DisplayQueue = xQueueCreate(3, sizeof(DisplayData));
QueueHandle_t AudioQueue = xQueueCreate(3, sizeof(AudioData));

// PRIO_GT911 touchp = PRIO_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_WIDTH, TOUCH_HEIGHT);

// Interrupt routine just sets a flag when rotation is detected
void IRAM_ATTR checkVolume()
{
    rotaryInstance.rotaryEncoder = true;
}

TaskHandle_t scrollerTaskHandle;  // Task handle for the scroller task
TaskHandle_t displayTaskHandle;   // Task handle for the TFT task
TaskHandle_t audioTaskHandle;     // Task handle for the audio task
TaskHandle_t webServerTaskHandle; // Task handle for the webserver task

// void PrioTftTask(void *parameter)
// {
//     while (true)
//     {
//         if (rotaryInstance.current_value_changed)
//         {
//             Serial.println("Volume changed to: " + String(rotaryInstance.current_value));
//             prioTft.setVolume(rotaryInstance.current_value);
//             myPrefs.writeValue("volume", rotaryInstance.current_value);
//             audio.setVolume(rotaryInstance.ReadRotaryValue());
//         }

//         int logoIndex;
//         if (xQueueReceive(logoQueue, &logoIndex, 0) == pdTRUE)
//         {
//             const String logo = UrlManagerInstance.Streams[logoIndex].logo;
//             prioTft.setLogo(logo);
//         }
//         vTaskDelay(10 / portTICK_PERIOD_MS); // Adjust the delay as needed (e.g., 10ms)
//     }
// }

// void VolumeTask(void *parameter)
// {
//     while (true)
//     {
//         rotaryInstance.loop();
//         if (rotaryInstance.current_value_changed)
//         {
//             Serial.println("Volume changed to: " + String(rotaryInstance.current_value));
//             myPrefs.writeValue("volume", rotaryInstance.current_value);
//             audio.setVolume(rotaryInstance.ReadRotaryValue());
//             displayData.volume = rotaryInstance.current_value;
//             audioData.volume = rotaryInstance.current_value;
//             xQueueSend(AudioQueue, &rotaryInstance.current_value, portMAX_DELAY);
//             xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
//         }
//         vTaskDelay(10 / portTICK_PERIOD_MS); // Adjust the delay as needed (e.g., 10ms)
//     }
// }



// Functie om de core van een taak te printen
void printTaskCore(TaskHandle_t taskHandle, const char *taskName)
{
    BaseType_t coreID = xTaskGetCoreID(taskHandle);
    if (coreID == tskNO_AFFINITY)
    {
        Serial.printf("Task %s is not pinned to any core.\n", taskName);
    }
    else
    {
        Serial.printf("Task %s is running on core %d.\n", taskName, coreID);
    }
}

void setup()
{
    Serial.begin(115200); // Initialize serial communication
    WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP
    WiFiManager wm;
    bool res;
    res = wm.autoConnect("espprio"); // password protected ap
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

        /* start touch */
        // touchp.begin();
        // touchp.setRotation(ROTATION_LEFT);
        //        audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

        myPrefs.begin();
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
        //  xQueueSend(streamQueue, &stream_index, portMAX_DELAY);

        // audioQueue.VolumeQ = xQueueCreate(3, sizeof(int));
        // audioQueue.StreamQ = xQueueCreate(3, sizeof(String));

        //       audio.setVolume(last_volume);
        //       audio.setVolumeSteps(VOLUME_STEPS);
        max_volume = 30; // todo

        audioData.volume = last_volume;
        strncpy(audioData.url, UrlManagerInstance.Streams[stream_index].url.c_str(), sizeof(audioData.url));
        xQueueSend(AudioQueue, &audioData, portMAX_DELAY);

        /* Rotary button */
        pinMode(ROT_CLK_PIN, INPUT);
        pinMode(ROT_DT_PIN, INPUT);
        rotary_button.setDebounceTime(50); // set debounce time to 50 milliseconds
        attachInterrupt(digitalPinToInterrupt(ROT_CLK_PIN), checkVolume, CHANGE);
        attachInterrupt(digitalPinToInterrupt(ROT_DT_PIN), checkVolume, CHANGE);
        rotaryInstance.begin(MIN_VOLUME, max_volume, DEF_VOLUME);
        rotaryInstance.current_value = last_volume;

        /* Next Button */
        pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
        next_button.setDebounceTime(150);
        next_button_state = next_button.getState();

        Serial.println("ESP32 TFT start...");

        // Set default display data
        strncpy(displayData.ip, WiFi.localIP().toString().c_str(), sizeof(displayData.ip));
        displayData.volume = last_volume;
        strncpy(displayData.title, "Prio-WebRadio", sizeof(displayData.title));
        strncpy(displayData.logo, UrlManagerInstance.Streams[stream_index].logo.c_str(), sizeof(displayData.logo));
        strncpy(displayData.bitrate, "unknown", sizeof(displayData.bitrate));
        strncpy(displayData.station, "unknown", sizeof(displayData.station));
        strncpy(displayData.icyurl, "unknown", sizeof(displayData.icyurl));
        strncpy(displayData.lasthost, "unknown", sizeof(displayData.lasthost));
        strncpy(displayData.streamtitle, "unknown", sizeof(displayData.streamtitle));

        xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);

        Serial.println("Starting tasks");

        // Create the FreeRTOS task for the VolumeTask
        // xTaskCreatePinnedToCore(
        //     AudioTask,            // Task function
        //     "AudioTask",          // Name of the task
        //     8192,                 // Stack size in words
        //     NULL,                 // Task parameter
        //     2,                    // Priority of the task
        //     &audioTaskHandle, 1); // Task handle

        // xTaskCreate(
        //     _AudioTask,   // Task function
        //     "_AudioTask", // Name of the task
        //     8192,         // Stack size in words
        //     NULL,         // Task parameter
        //     1,            // Priority of the task
        //     NULL);        // Task handle

        // // Create the FreeRTOS task for the VolumeTask
        // xTaskCreatePinnedToCore(
        //     VolumeTask,          // Task function
        //     "VolumeTask",        // Name of the task
        //     4096,                // Stack size in words
        //     NULL,                // Task parameter
        //     5,                   // Priority of the task
        //     &scrollerTaskHandle, // Task handle
        //     0);

        //   Create the FreeRTOS task for the VolumeTask
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
          

        // Create the FreeRTOS task for the VolumeTask
        xTaskCreatePinnedToCore(
            AudioTask,            // Task function
            "AudioTask",          // Name of the task
            8192,                 // Stack size in words
            (void *)AudioQueue,   // Task parameter
            1,                    // Priority of the task
            &audioTaskHandle, 1); // Task handle
     }
}

void oldloop() {
    Serial.println("ESP draait...");
    delay(1000);
}

void loop()
{

    next_button.loop();
    next_button_state = next_button.getState();
    if (next_button.isPressed() && next_button_isreleased == true)
    {
        Serial.println("The button is pressed");
        Serial.println(displayData.logo);
        Serial.println(displayData.ip);
        next_button_isreleased = false;

        if (stream_index == UrlManagerInstance.streamCount - 1)
        {
            stream_index = 0;
        }
        else
        {
            stream_index++;
        }
        Serial.println("Switching stream! ");
        CreateAndSendAudioData(stream_index, rotaryInstance.current_value);
        CreateAndSendDisplayData(stream_index);
    }

    if (next_button.isReleased())
    {
        /* we only will listen to button input if it was released */
        next_button_isreleased = true;
        Serial.println("The button is released");
        Serial.println(current_url);
    }

    rotaryInstance.loop();
    if (rotaryInstance.current_value_changed)
    {
        Serial.println("Volume changed to: " + String(rotaryInstance.current_value));
        myPrefs.writeValue("volume", rotaryInstance.current_value);
        displayData.volume = rotaryInstance.current_value;
        audioData.volume = rotaryInstance.current_value;
        xQueueSend(AudioQueue, &audioData, portMAX_DELAY);
        xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
        rotaryInstance.current_value_changed = false;
    }
    vTaskDelay(1 / portTICK_PERIOD_MS); // Adjust the delay as needed (e.g., 10ms)
} // end loop

void CreateAndSendDisplayData(int streamIndex)
{
    displayData.volume = last_volume;
    strncpy(displayData.ip, WiFi.localIP().toString().c_str(), sizeof(displayData.ip));
    strncpy(displayData.title, UrlManagerInstance.Streams[streamIndex].name.c_str(), sizeof(displayData.title)); // -1 voor mogelijke \0
    strncpy(displayData.logo, UrlManagerInstance.Streams[streamIndex].logo.c_str(), sizeof(displayData.logo));
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
}

void CreateAndSendAudioData(int streamIndex, int last_volume)
{
    audioData.volume = last_volume;
    strncpy(audioData.url, UrlManagerInstance.Streams[streamIndex].url.c_str(), sizeof(audioData.url));
    xQueueSend(AudioQueue, &audioData, portMAX_DELAY);
}

// // optional
// void audio_info(const char *info)
// {
//     Serial.print("info        ");
//     Serial.println(info);
// }
void audio_showstation(const char *info)
{
    Serial.print("station:        ");
    Serial.println(info);
    strncpy(displayData.station, info, sizeof(info)); // -1 voor mogelijke \0
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
}
void audio_showstreamtitle(const char *info)
{
    strncpy(displayData.streamtitle, info, sizeof(displayData.streamtitle) - 1);
    displayData.streamtitle[sizeof(displayData.streamtitle) - 1] = '\0'; // Zorg ervoor dat de string null-terminated is
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
}
void audio_bitrate(const char *info)
{
    Serial.print("bitrate:        ");
    Serial.println(info);
    strncpy(displayData.bitrate, info, sizeof(displayData.bitrate)-1);
    displayData.bitrate[sizeof(displayData.bitrate) - 1] = '\0'; // Zorg ervoor dat de string null-terminated is
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
}
// void audio_icyurl(const char *info)
// {
//     strncpy(displayData.icyurl, info, sizeof(displayData.icyurl - 1)); // -1 voor mogelijke \0
//     xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
// }
// void audio_lasthost(const char *info)
// {
//     strncpy(displayData.lasthost, info, sizeof(displayData.lasthost - 1)); // -1 voor mogelijke \0
//     xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
// }
