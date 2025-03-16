// vTask explained -> https://www.youtube.com/watch?v=ywbq1qR-fY0

#include "main.h"
#include <WiFiManager.h>
#include "PrioRotary.h"
#include <ezButton.h> // the library to use for SW pin
#include "MyPreferences.h"
#include "PrioWebserver.h"
#include "UrlManager.h"
#include "PrioTft.h"
#include "Free_Fonts.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/event_groups.h>
#include "task_webServer.h"
#include "Task_Audio.h"
#include "Task_Display.h"
#include "Task_Shared.h"

// #include <PRIO_GT911.h>

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

String default_url = "https://icecast.omroep.nl/radio1-bb-mp3:443";
String last_url;

PrioRotary rotaryInstance(ROT_CLK_PIN, ROT_DT_PIN);
MyPreferences myPrefs("myRadio");
UrlManager UrlManagerInstance(myPrefs);
PrioWebServer webServer(UrlManagerInstance, 80);
DisplayData displayData;
AudioData audioData;
PrioRfReceiver rfReceiver(RF_RECEIVER_PIN); // Gebruik GPIO 23 voor de RF-ontvanger

/* Buttons */
ezButton rotary_button(ROT_SW_PIN);
// ezButton next_button(NEXT_BUTTON_PIN);



// Queues
QueueHandle_t logoQueue = xQueueCreate(3, sizeof(int));
QueueHandle_t streamQueue = xQueueCreate(3, sizeof(int));
QueueHandle_t volumeQueue = xQueueCreate(3, sizeof(int));
QueueHandle_t DisplayQueue = xQueueCreate(3, sizeof(DisplayData));
QueueHandle_t AudioQueue = xQueueCreate(3, sizeof(AudioData));

// Maak een event group
EventGroupHandle_t taskEvents; // Event group handle for starting order of the tasks

// PRIO_GT911 touchp = PRIO_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_WIDTH, TOUCH_HEIGHT);

// Maak een instantie van de PrioInputPanel class
PrioInputPanel inputPanel(INPUTPANEL_ADDRESS, INPUTPANEL_INT_PIN, INPUTPANEL_SDA, INPUTPANEL_SCL);



// Interrupt routine just sets a flag when rotation is detected
void IRAM_ATTR checkVolume()
{
    rotaryInstance.rotaryEncoder = true;
}


TaskHandle_t scrollerTaskHandle;  // Task handle for the scroller task
TaskHandle_t displayTaskHandle;   // Task handle for the TFT task
TaskHandle_t audioTaskHandle;     // Task handle for the audio task
TaskHandle_t webServerTaskHandle; // Task handle for the webserver task

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


        inputPanel.begin(); // Initialiseer de input panel
        inputPanel.setButtonPressedCallback(onButtonPressed); // Stel de callback in

        // Initialiseer de RF-ontvanger
        rfReceiver.begin();
        rfReceiver.setKeyReceivedCallback(onRfButtonPressed); // Stel de callback-functie in


        
        Serial.println("ESP32 TFT start...");

        // Set default display data and put it queue
        strncpy(displayData.ip, WiFi.localIP().toString().c_str(), sizeof(displayData.ip));
        displayData.volume = last_volume;
        CreateAndSendDisplayData(stream_index);

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

uint8_t button_counter = 0;

void loop()
{

    
    inputPanel.loop(); // Voer de loop van de input panel uit
    rfReceiver.loop(); // Voer de loop van de RF-ontvanger uit
 
    rotaryInstance.loop();
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
    vTaskDelay(1 / portTICK_PERIOD_MS); // Adjust the delay as needed (e.g., 10ms)
} // end loop


void usePreset(int preset)
{
    stream_index = preset;
    CreateAndSendAudioData(stream_index, rotaryInstance.current_value);
    CreateAndSendDisplayData(stream_index);
}

// Callback functie voor button-drukken
void onButtonPressed(int buttonIndex) {
    usePreset(buttonIndex); // Roep de usePreset functie aan
  }

  void onRfButtonPressed(int KeyIndex) {
    usePreset(KeyIndex); // Roep de usePreset functie aan
  }




// Voorbeeld implementatie van usePreset
// void usePreset(int buttonPressed) {
//     Serial.print("Preset gebruikt: ");
//     Serial.println(buttonPressed);
//   }
void CreateAndSendDisplayData(int streamIndex)
{
    displayData.volume = last_volume;
    strncpy(displayData.ip, WiFi.localIP().toString().c_str(), sizeof(displayData.ip));
    strncpy(displayData.title, UrlManagerInstance.Streams[streamIndex].name.c_str(), sizeof(displayData.title)); // -1 voor mogelijke \0
    strncpy(displayData.logo, UrlManagerInstance.Streams[streamIndex].logo.c_str(), sizeof(displayData.logo));
    strncpy(displayData.bitrate, "Loading...", sizeof(displayData.bitrate));
    strncpy(displayData.station, "Loading...", sizeof(displayData.station));
    strncpy(displayData.icyurl, "Loading...", sizeof(displayData.icyurl));
    strncpy(displayData.lasthost, "Loading...", sizeof(displayData.lasthost));
    strncpy(displayData.streamtitle, "Loading...", sizeof(displayData.streamtitle));
    xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
}

void CreateAndSendAudioData(int streamIndex, int last_volume)
{
    Serial.println("CreateAndSendAudioData");
    audioData.volume = last_volume;
    strncpy(audioData.url, UrlManagerInstance.Streams[streamIndex].url.c_str(), sizeof(audioData.url));
    xQueueSend(AudioQueue, &audioData, portMAX_DELAY);
}

void printBinary(int v, int num_places) {
	int mask = 0, n;

	for (n = 1; n <= num_places; n++) {
		mask = (mask << 1) | 0x0001;
	}
	v = v & mask;  // truncate v to specified number of places

	while (num_places) {

		if (v & (0x0001 << (num_places - 1))) {
			Serial.print("1");
		}
		else {
			Serial.print("0");
		}

		--num_places;
		if (((num_places % 4) == 0) && (num_places != 0)) {
			Serial.print("_");
		}
	}
	Serial.println("");
}

// // optional
// void audio_info(const char *info)
// {
//     Serial.print("info        ");
//     Serial.println(info);
// }
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
