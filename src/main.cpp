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

#include "PCF8575.h"

// Set i2c address
PCF8575 pcf8575(0x20);

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

/* Buttons */
ezButton rotary_button(ROT_SW_PIN);
// ezButton next_button(NEXT_BUTTON_PIN);

// Array om de status van de buttons bij te houden
volatile bool buttonPressed[10] = {false};
volatile bool min_one_preset_ispressed = false;
volatile bool next_button_ispressed = false;
volatile bool preset_buttonPressed = false;
// Array om de laatste tijd van button-drukken bij te houden
volatile unsigned long next_lastDebounceTime = 0;

// Debounce-tijd in milliseconden
const unsigned long debounceDelay = 100;

// Queues
QueueHandle_t logoQueue = xQueueCreate(3, sizeof(int));
QueueHandle_t streamQueue = xQueueCreate(3, sizeof(int));
QueueHandle_t volumeQueue = xQueueCreate(3, sizeof(int));
QueueHandle_t DisplayQueue = xQueueCreate(3, sizeof(DisplayData));
QueueHandle_t AudioQueue = xQueueCreate(3, sizeof(AudioData));

// Maak een event group
EventGroupHandle_t taskEvents; // Event group handle for starting order of the tasks

// PRIO_GT911 touchp = PRIO_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_WIDTH, TOUCH_HEIGHT);

// Interrupt routine just sets a flag when rotation is detected
void IRAM_ATTR checkVolume()
{
    rotaryInstance.rotaryEncoder = true;
}

// Interrupt Service Routine (ISR)
void IRAM_ATTR buttonISR()
{

    min_one_preset_ispressed = true;
    // Lees de status van alle buttons en zet de juiste vlag
    for (int i = 0; i < 10; i++)
    {
        if (digitalRead(buttonPins[i]) == LOW)
        {
            buttonPressed[i] = true;
            break;
        }
    }
}

void IRAM_ATTR preset_buttonISR()
{

    preset_buttonPressed = true;
}

void IRAM_ATTR next_buttonISR()
{

    // unsigned long currentTime = millis();
    // if (currentTime - next_lastDebounceTime >= debounceDelay)
    // {
    //     next_button_ispressed = true;
    //     next_lastDebounceTime = currentTime; // Update de laatste tijd
    // }
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

        /* Next Button */
        // pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
        // next_button.setDebounceTime(50);
        // next_button_state = next_button.getState();

        // pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
        // // Voeg een interrupt toe aan pin voor next_button
        // attachInterrupt(digitalPinToInterrupt(NEXT_BUTTON_PIN), next_buttonISR, FALLING);

        // I2C initialiseren
        Wire.begin();

        // Interrupt pin configureren
        pinMode(PCF8575_INT_PIN, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(PCF8575_INT_PIN), preset_buttonISR, FALLING);

        // // PCF8575 configureren: alle pinnen als input
        // Wire.beginTransmission(PCF8575_ADDRESS);
        // Wire.write(0xFF); // Alle pinnen als input (1 = input)
        // Wire.write(0xFF); // Tweede byte voor PCF8575 (16 pinnen)
        // Wire.endTransmission();

        pcf8575.pinMode(P0, INPUT);

      
        pcf8575.begin();

        // Configureer de GPIO pinnen als input met pull-up weerstand
        for (int i = 0; i < 10; i++)
        {
            pinMode(buttonPins[i], INPUT_PULLUP);
            // Voeg een interrupt toe aan elke pin
            attachInterrupt(digitalPinToInterrupt(buttonPins[i]), buttonISR, FALLING);
        }

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

    // next_button.loop();
    // Controleer of een button is ingedrukt

    uint8_t val = pcf8575.digitalRead(P0);
    if (val == HIGH)
    {

      Serial.println("P0 is HIGH");
  
    }

    if (preset_buttonPressed)
    {
        // Lees de buttons
        readPresetButtons();
        // Reset de vlag
        preset_buttonPressed = false;
  
    }
    if (min_one_preset_ispressed)
    {
        noInterrupts(); // Schakel interrupts uit, zodat de vlaggen niet worden overschreven
        min_one_preset_ispressed = false;
        interrupts(); // Schakel interrupts weer in
        // Loop door alle buttons
        for (int i = 0; i < 10; i++)
        {
            if (buttonPressed[i])
            {
                // Reset de vlag
                buttonPressed[i] = false;
                // Voer de gewenste actie uit
                usePreset(i);
                break;
            }
        }
    }
    // next_button_state = next_button.getState();
    // if (next_button.isPressed())
    // if (next_button_ispressed)
    // {
    //     noInterrupts();
    //     next_button_ispressed = false;
    //     interrupts();
    //     Serial.println("Next button pressed");
    //     if (stream_index == UrlManagerInstance.streamCount - 1)
    //     {
    //         stream_index = 0;
    //     }
    //     else
    //     {
    //         stream_index++;
    //     }
    //     Serial.println("Switching stream! ");
    //     CreateAndSendAudioData(stream_index, rotaryInstance.current_value);
    //     CreateAndSendDisplayData(stream_index);
    // }

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

// Functie om de buttons uit te lezen
void readPresetButtons() {
    // Lees de status van de PCF8575
    Wire.beginTransmission(PCF8575_ADDRESS);
    Wire.requestFrom(PCF8575_ADDRESS, 2); // Lees 2 bytes (16 pinnen)
  
    // Lees de eerste byte (P0-P7)
    uint8_t buttonStates1 = Wire.read();
    // Lees de tweede byte (P8-P15)
    uint8_t buttonStates2 = Wire.read();
    Wire.endTransmission();
  
    // Combineer de twee bytes tot een 16-bit waarde
    uint16_t buttonStates = (buttonStates2 << 8) | buttonStates1;
  Serial.print("buttonStates: ");
    printBinary(buttonStates, 16);
    // Controleer welke button is ingedrukt
    for (int i = 0; i < 10; i++) {
      if (!(buttonStates & (1 << i))) { // Als de pin laag is (button ingedrukt)
        usePreset(i); // Voer de bijbehorende actie uit
        break; // Stop na de eerste gedrukte button
      }
    }
  }

void usePreset(int preset)
{
    stream_index = preset;
    CreateAndSendAudioData(stream_index, rotaryInstance.current_value);
    CreateAndSendDisplayData(stream_index);
}
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
