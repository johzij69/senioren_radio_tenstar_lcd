#include "main.h"
#include <WiFiManager.h>
#include "Audio.h"
#include "PrioRotary.h"
#include <ezButton.h> // the library to use for SW pin
#include "MyPreferences.h"
#include "PrioWebserver.h"
#include "UrlManager.h"
#include "PrioTft.h"
#include <TFT_eSPI.h>
#include "PrioBar.h"
#include "StreamLogo.h"
#include "PrioScrollText.h"
#include "Free_Fonts.h"
#include "TFTScroller.h"
// #include "smallFont.h"  // bar
// #include "middleFont.h" // bar
// #include <SPIFFS.h> // Include the SPIFFS library
// #include <PRIO_GT911.h>

#define WM_ERASE_NVS

// DAC I2S
#define I2S_DOUT 7
#define I2S_BCLK 5
#define I2S_LRC 6

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
int stream_index = 0;
int next_button_state = 0;

const char *current_url;

bool next_button_isreleased = true;

String default_url = "https://icecast.omroep.nl/radio1-bb-mp3:443";
String last_url;

Audio audio;
PrioRotary rotaryInstance(ROT_CLK_PIN, ROT_DT_PIN);
ezButton rotary_button(ROT_SW_PIN);
ezButton next_button(NEXT_BUTTON_PIN);
MyPreferences myPrefs("myRadio");
UrlManager UrlManagerInstance(myPrefs);
PrioWebServer webServer(UrlManagerInstance, 80);
PrioTft prioTft;
TFT_eSPI tft = TFT_eSPI();
// PrioBar pBar(&tft);
StreamLogo strLogo(&tft);
ScrollText *st_streamTitle;
TFTScroller scroller(tft, 20, 60, 100, 450, FM12);

// PRIO_GT911 touchp = PRIO_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_WIDTH, TOUCH_HEIGHT);

// Interrupt routine just sets a flag when rotation is detected
void IRAM_ATTR checkVolume()
{
    rotaryInstance.rotaryEncoder = true;
}

TaskHandle_t scrollerTaskHandle; // Task handle for the scroller task
TaskHandle_t priotftTaskHandle;  // Task handle for the TFT task
TaskHandle_t audioTaskHandle;  // Task handle for the audio task

void PrioTftTask(void *parameter)
{
    while (true)
    {
        if (rotaryInstance.current_value_changed)
        {
            Serial.println("Volume changed to: " + String(rotaryInstance.current_value));
            prioTft.setVolume(rotaryInstance.current_value);
            myPrefs.writeValue("volume", rotaryInstance.current_value);
            audio.setVolume(rotaryInstance.ReadRotaryValue());
        }
        prioTft.loop();
        vTaskDelay(10 / portTICK_PERIOD_MS); // Adjust the delay as needed (e.g., 10ms)
    }
}

void VolumeTask(void *parameter)
{
    while (true)
    {
        rotaryInstance.loop();
        vTaskDelay(10 / portTICK_PERIOD_MS); // Adjust the delay as needed (e.g., 10ms)
    }
}

void AudioTask(void *parameter)
{
    while (true)
    {
        audio.loop();
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
        audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

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
        last_url = UrlManagerInstance.Streams[stream_index].url;
        current_url = last_url.c_str();

        if (last_url != "")
        {
            audio.connecttohost(last_url.c_str());
        }
        else
        {
            audio.connecttohost("https://stream.100p.nl/100pctnl.mp3");
        }

        audio.setVolume(last_volume);
        audio.setVolumeSteps(VOLUME_STEPS);
        max_volume = audio.maxVolume();

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

        /* Start webserver */
        webServer.ip = WiFi.localIP().toString();
        webServer.begin();
        Serial.println("ESP32 TFT start...");
        prioTft.begin(); // Initialiseer het TFT scherm
        if (!prioTft.isInitialized)
        {
            Serial.println("TFT-initialisatie mislukt. Controleer hardwareverbindingen.");
        }
        else
        {
            prioTft.last_volume = last_volume;
            prioTft.cur_volume = last_volume;
            Serial.println("TFT succesvol geïnitialiseerd!");

            // pBar.begin(450, 0, 30, 320, TFT_LIGHTGREY, TFT_BLACK, max_volume);
            // pBar.draw(last_volume);

            strLogo.begin();
            // Set default logo
            if (UrlManagerInstance.Streams[stream_index].logo != "")
            {
                strLogo.Show(35, 100, UrlManagerInstance.Streams[stream_index].logo.c_str());
            }
            else
            {
                strLogo.Show(35, 100, "https://img.prio-ict.nl/api/images/webradio-default.jpg");
            }

            webServer.ip = WiFi.localIP().toString();

            st_streamTitle = new ScrollText(tft, "", 20, 80, 420, 40, FMBO12);
            st_streamTitle->setTextBetweenSpace(20);
            st_streamTitle->setScrollDelay(5000);
            st_streamTitle->setScrollType(ScrollType::RIGHT_TO_LEFT_CONT);
            st_streamTitle->setScrollSpeed(10);
            st_streamTitle->begin();
            st_streamTitle->setText("initializing...");
            st_streamTitle->setText("Dit is een lange tekst die zal scrollen, zodat je de hele tekst kunt lezen.");


  // Create the FreeRTOS task for the VolumeTask
            xTaskCreate(
                AudioTask,         // Task function
                "AudioTask",       // Name of the task
                4096,                // Stack size in words
                NULL,                // Task parameter
                1,                   // Priority of the task
                &audioTaskHandle); // Task handle
            // Create the FreeRTOS task for the VolumeTask
            xTaskCreate(
                VolumeTask,           // Task function
                "VolumeTask",         // Name of the task
                4096,                 // Stack size in words
                NULL,                 // Task parameter
                5,                    // Priority of the task
                &scrollerTaskHandle); // Task handle

            prioTft.showLocalIp(WiFi.localIP().toString());

            // Create the FreeRTOS task for the VolumeTask
            xTaskCreate(
                PrioTftTask,         // Task function
                "PrioTftTask",       // Name of the task
                4096,                // Stack size in words
                NULL,                // Task parameter
                4,                   // Priority of the task
                &priotftTaskHandle); // Task handle
        }
    }
}

void loop()
{

 //   audio.loop();
    next_button.loop();

    // if (rotaryInstance.current_value_changed)
    // {
    //     Serial.println("Volume changed to: " + String(rotaryInstance.current_value));
    //     prioTft.draw(rotaryInstance.current_value);
    //     audio.setVolume(rotaryInstance.ReadRotaryValue());
    //     myPrefs.writeValue("volume", rotaryInstance.current_value);
    // }

    next_button_state = next_button.getState();

    if (next_button.isPressed() && next_button_isreleased == true)
    {
        Serial.println("The button is pressed");
        next_button_isreleased = false;
        if (stream_index == UrlManagerInstance.streamCount - 1)
        {
            stream_index = 0;
        }
        else
        {
            stream_index++;
        }
        current_url = UrlManagerInstance.Streams[stream_index].url.c_str();

        myPrefs.writeString("lasturl", current_url);
        myPrefs.putUInt("lastStreamIndex", stream_index);

        Serial.print("stream count: ");
        Serial.println(UrlManagerInstance.streamCount);
        Serial.print("stream index: ");
        Serial.println(stream_index);
        Serial.println("playing:");
        Serial.println(current_url);
        Serial.print("stream logo: ");
        Serial.println(UrlManagerInstance.Streams[stream_index].logo);
        audio.connecttohost(current_url);
        strLogo.Show(35, 100, UrlManagerInstance.Streams[stream_index].logo);
    }

    if (next_button.isReleased())
    {
        /* we only will listen to button input if it was released */
        next_button_isreleased = true;
        Serial.println("The button is released");
        Serial.println(current_url);
    }

    // st_streamTitle->update();

    //  vTaskDelay(1);
}

// optional
void audio_info(const char *info)
{
    Serial.print("info        ");
    Serial.println(info);
}
void audio_id3data(const char *info)
{ // id3 metadata
    Serial.print("id3data     ");
    Serial.println(info);
}
void audio_eof_mp3(const char *info)
{ // end of file
    Serial.print("eof_mp3     ");
    Serial.println(info);
}
void audio_showstation(const char *info)
{
    Serial.print("station     ");
    Serial.println(info);
}
void audio_showstreamtitle(const char *info)
{
    Serial.print("streamtitle ");
    Serial.println(info);
    //      st_streamTitle->setText(info);
}
void audio_bitrate(const char *info)
{
    Serial.print("bitrate     ");
    Serial.println(info);
}
void audio_commercial(const char *info)
{ // duration in sec
    Serial.print("commercial  ");
    Serial.println(info);
}
void audio_icyurl(const char *info)
{ // homepage
    Serial.print("icyurl      ");
    Serial.println(info);
}
void audio_lasthost(const char *info)
{ // stream URL played
    Serial.print("lasthost    ");
    Serial.println(info);
}
void audio_eof_speech(const char *info)
{
    Serial.print("eof_speech  ");
    Serial.println(info);
}