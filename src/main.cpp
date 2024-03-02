#include "main.h"

#include "WiFi.h"
#include <WiFiManager.h>
#include <AsyncTCP.h>
#include "PrioWebserver.h"

#include "SPI.h"
// #include "FS.h"
// #include "SD.h"
#include "vs1053_ext.h"
#include <ezButton.h> // the library to use for SW pin
// #include "player.h"
#include "MyPreferences.h"
#include "UrlManager.h"
#include "PrioRotary.h"

#define VS1053_MOSI 23
#define VS1053_MISO 19
#define VS1053_SCK 18

#define VS1053_CS 5
#define VS1053_DCS 16
#define VS1053_DREQ 4

#define CLK_PIN 25 // ESP32 pin GPIO25 connected to the rotary encoder's CLK pin
#define DT_PIN 26  // ESP32 pin GPIO26 connected to the rotary encoder's DT pin
#define SW_PIN 27  // ESP32 pin GPIO27 connected to the rotary encoder's SW pin

#define DIRECTION_CW 0  // clockwise direction
#define DIRECTION_CCW 1 // counter-clockwise direction

#define NEXT_BUTTON_PIN 32 // ESP32 pin GPIO18, which connected to button

#define MAX_VOLUME 50
#define MIN_VOLUME 0
#define DEF_VOLUME 20

int counter_volume = DEF_VOLUME;
int stream_index = 0;
int max_volume = 100;
int min_volume = 0;
int direction = DIRECTION_CW;
int CLK_state;
int prev_CLK_state;
int next_button_state = 0; // variable for reading the button status

ezButton button(SW_PIN); // create ezButton object that attach to pin 27;
ezButton next_button(NEXT_BUTTON_PIN);

// TaskHandle_t Task1;

// Play player(VS1053_CS, VS1053_DCS, VS1053_DREQ);
VS1053 mp3(VS1053_CS, VS1053_DCS, VS1053_DREQ, VSPI, VS1053_MOSI, VS1053_MISO, VS1053_SCK);
// VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ, UNDEFINED, SPI);
MyPreferences myPrefs("myRadio");

UrlManager UrlManagerInstance(myPrefs);

PrioWebServer webServer(UrlManagerInstance, 80);

PrioRotary rotaryInstance(CLK_PIN, DT_PIN);

String default_url = "https://icecast.omroep.nl/radio1-bb-mp3:443";

const char *current_url;

String piet;

// A turn counter for the rotary encoder (negative = anti-clockwise)
int rotationCounter = 200;

// Flag from interrupt routine (moved=true)
volatile bool rotaryEncoder = false;

// Interrupt routine just sets a flag when rotation is detected
void IRAM_ATTR rotary()
{
  rotaryEncoder = true;
}

// Interrupt routine just sets a flag when rotation is detected
void IRAM_ATTR checkVolume()
{
  rotaryInstance.rotaryEncoder = true;
}

/

void setup()
{
  Serial.begin(115200);

  //    SPI.setHwCs(true);

  // Wait for VS1053 and PAM8403 to power up
  // otherwise the system might not start up correctly

  SPI.begin(VS1053_SCK, VS1053_MISO, VS1053_MOSI);
  delay(3000);
  // xTaskCreatePinnedToCore(
  //     Task1code, // Function to implement the task
  //     "Task1",   // Name of the task
  //     1000,      // Stack size in bytes
  //     NULL,      // Task input parameter
  //     0,         // Priority of the task
  //     &Task1,    // Task handle.
  //     0          // Core where the task should run
  // );

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
                       // configure encoder pins as inputs
  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN, INPUT);
  button.setDebounceTime(50); // set debounce time to 50 milliseconds

  // read the initial state of the rotary encoder's CLK pin
  prev_CLK_state = digitalRead(CLK_PIN);

  next_button_state = next_button.getState();

  // pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
  next_button.setDebounceTime(50);

  // This can be set in the IDE no need for ext library
  // system_update_cpu_freq(160);

  // Gebruik de klasse in de main

  Serial.println("\n\nSimple Radio Node WiFi Radio");

  WiFiManager wm;
  // wm.setSaveParamsCallback(saveParamCallback);

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  res = wm.autoConnect("espprio"); // password protected ap

  if (!res)
  {
    Serial.println("Failed to connect");
    return;
  }
  else
  {
    // if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("DNS address: ");
    Serial.println(WiFi.dnsIP());
    Serial.print("Gateway address: ");
    Serial.println(WiFi.gatewayIP());

    myPrefs.begin();
    UrlManagerInstance.begin();

    // urlManager.addUrl("https://icecast.omroep.nl/radio1-bb-mp3:443");
    // urlManager.addUrl("https://icecast.omroep.nl/radio2-bb-mp3:443");
    // urlManager.addUrl("https://icecast.omroep.nl/3fm-bb-mp3:443");

    UrlManagerInstance.addUrl("https://icecast.omroep.nl/radio1-bb-mp3:443");
    UrlManagerInstance.addUrl("https://icecast.omroep.nl/radio2-bb-mp3:443");
    UrlManagerInstance.addUrl("https://icecast.omroep.nl/3fm-bb-mp3:443");

    // Druk alle URLs af vanuit AnotherClass
    UrlManagerInstance.printAllUrls();

    piet = myPrefs.readString("lasturl", default_url.c_str());
    current_url = piet.c_str();

    Serial.println(piet);

    Serial.print("Huidige url: ");
    Serial.println(current_url);
    Serial.print("Aantal urls: ");
    Serial.println(UrlManagerInstance.urlCount);

    // player.begin();
    // player.switchToMp3Mode();
    // int ivol = myPrefs.readValue("volume", VOLUME);
    // player.setVolume(ivol);
    // player.play(current_url);

    Serial.println("mp3 begin");
    mp3.begin();
    Serial.println("set volume steps");

    mp3.setVolumeSteps(MAX_VOLUME);
    Serial.println("set volume");
    mp3.setVolume(DEF_VOLUME);
    Serial.println("conecting");
    // mp3.connecttohost("https://icecast.omroep.nl/radio1-bb-mp3");
    // mp3.connecttohost("https://20133.live.streamtheworld.com/100PNL_MP3_SC");
    mp3.connecttohost("https://stream.100p.nl/100pctnl.mp3");
    Serial.println("conected");

    webServer.begin();

    // rotary encoder interrupts
    //  We need to monitor both pins, rising and falling for all states
    //  attachInterrupt(digitalPinToInterrupt(CLK_PIN), rotary, CHANGE);
    //  attachInterrupt(digitalPinToInterrupt(DT_PIN), rotary, CHANGE);

    attachInterrupt(digitalPinToInterrupt(CLK_PIN), checkVolume, CHANGE);
    attachInterrupt(digitalPinToInterrupt(DT_PIN), checkVolume, CHANGE);
    rotaryInstance.begin();
    mp3.setVolume(rotaryInstance.ReadRotaryValue());
    // Set de functiepointer vanuit de main
    //  rotaryInstance.useValuePointer = mp3.setVolume;
  }
}
void loop()
{
  mp3.loop();
  rotaryInstance.loop();
  if (rotaryInstance.rotary_value_changed)
  {
    mp3.setVolume(rotaryInstance.ReadRotaryValue());
  }

  // if (rotaryEncoder)
  // {
  //   // Get the movement (if valid)
  //   int8_t rotationValue = checkRotaryEncoder();

  //   // If valid movement, do something
  //   if (rotationValue != 0)
  //   {

  //     if (counter_volume > MIN_VOLUME && counter_volume < MAX_VOLUME || counter_volume == MAX_VOLUME && rotationValue == -1 || counter_volume == MIN_VOLUME && rotationValue == 1)
  //     {
  //       rotationValue < 1 ? counter_volume-- : counter_volume++;
  //     }
  //     Serial.print("volume:");
  //     Serial.println(String(counter_volume));
  //     mp3.setVolume(counter_volume);
  //   }
  // }
}
//  void loop()
// {

//   // rotary encoder
//   button.loop(); // MUST call the loop() function first
//   next_button.loop();

//   // read the current state of the rotary encoder's CLK pin
//   CLK_state = digitalRead(CLK_PIN);

//   // If the state of CLK is changed, then pulse occurred
//   // React to only the rising edge (from LOW to HIGH) to avoid double count
//   if (CLK_state != prev_CLK_state && CLK_state == HIGH)
//   {
//     // if the DT state is HIGH
//     // the encoder is rotating in counter-clockwise direction => decrease the counter
//     if (digitalRead(DT_PIN) == HIGH)
//     {
//       if (counter_volume > 0)
//       {
//         counter_volume--;
//       }
//       direction = DIRECTION_CCW;
//     }
//     else
//     {
//       // the encoder is rotating in clockwise direction => increase the counter
//       if (counter_volume < 100)
//       {
//         counter_volume++;
//       }
//       direction = DIRECTION_CW;
//     }

//     Serial.print("Rotary Encoder:: direction: ");
//     if (direction == DIRECTION_CW)
//       Serial.print("Clockwise");
//     else
//       Serial.print("Counter-clockwise");

//     Serial.print(" - count: ");
//     Serial.println(counter_volume);
//     player.setVolume(counter_volume);
//     myPrefs.writeValue("volume", counter_volume);
//   }

//   // save last CLK state
//   prev_CLK_state = CLK_state;

//   if (button.isPressed())
//   {
//     Serial.println("The button is pressed");
//     Serial.println(String(myPrefs.readValue("volume", 80)));
//     Serial.println(myPrefs.readString("lasturl", String(" ").c_str()));
//   }

//   next_button_state = next_button.getState();

//   if (next_button.isPressed())
//   {
//     Serial.println("The button is pressed");
//     if (stream_index == UrlManagerInstance.urlCount - 1)
//     {
//       stream_index = 0;
//     }
//     else
//     {
//       stream_index++;
//     }
//     current_url = UrlManagerInstance.getUrlAtIndex(stream_index);
//     myPrefs.writeString("lasturl", current_url);
//     Serial.println("playing:");
//     Serial.println(current_url);
//     // player.resetStream();
//       player.play(current_url);
//   }

//   if (next_button.isReleased())
//     Serial.println("The button is released");

//   if (current_url != "")
//   {
//     player.play(current_url);
//   }
//   else
//   {
//     Serial.println("Url is empty, trying last");
//     current_url = readDefaultUrl().c_str();
//     delay(1000);
//   }
// }

// void Task1code(void *pvParameters)
// {
//   while (1)
//   {
//     // Serial.print("Hello");
//     delay(500); // wait for half a second
//     // Serial.print(" World from loop2() at ");
//     // Serial.println(xPortGetCoreID());
//     // delay(500); // wait for half a second
//   }
// }

String readDefaultUrl()
{

  String defaultUrl = "https://icecast.omroep.nl/radio1-bb-mp3:443";
  String result = myPrefs.readString("lasturl", defaultUrl.c_str());
  return result;
}

// next code is optional:
void vs1053_info(const char *info)
{ // called from vs1053
  Serial.print("DEBUG:        ");
  Serial.println(info); // debug infos
}
void vs1053_showstation(const char *info)
{ // called from vs1053
  Serial.print("STATION:      ");
  Serial.println(info); // Show station name
}
void vs1053_showstreamtitle(const char *info)
{ // called from vs1053
  Serial.print("STREAMTITLE:  ");
  Serial.println(info); // Show title
}
void vs1053_showstreaminfo(const char *info)
{ // called from vs1053
  Serial.print("STREAMINFO:   ");
  Serial.println(info); // Show streaminfo
}
void vs1053_eof_mp3(const char *info)
{ // called from vs1053
  Serial.print("vs1053_eof:   ");
  Serial.print(info); // end of mp3 file (filename)
}
void vs1053_bitrate(const char *br)
{ // called from vs1053
  Serial.print("BITRATE:      ");
  Serial.println(String(br) + "kBit/s"); // bitrate of current stream
}
void vs1053_commercial(const char *info)
{ // called from vs1053
  Serial.print("ADVERTISING:  ");
  Serial.println(String(info) + "sec"); // info is the duration of advertising
}
void vs1053_icyurl(const char *info)
{ // called from vs1053
  Serial.print("Homepage:     ");
  Serial.println(info); // info contains the URL
}
void vs1053_eof_speech(const char *info)
{ // called from vs1053
  Serial.print("end of speech:");
  Serial.println(info);
}
void vs1053_lasthost(const char *info)
{ // really connected URL
  Serial.print("lastURL:      ");
  Serial.println(info);
}