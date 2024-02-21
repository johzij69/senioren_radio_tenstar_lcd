
// #include <VS1053.h>

#include "main.h"
#include "Arduino.h"
#include "WiFi.h"
#include <WiFiManager.h>
#include <AsyncTCP.h>
#include "webhandler.h"

#include <SPI.h>
#include <Preferences.h>
#include "FS.h"
#include "SD.h"
#include <ezButton.h> // the library to use for SW pin
#include "player.h"
#include "urlmanager.h"

#define VS1053_CS 5
#define VS1053_DCS 16
#define VS1053_DREQ 4

#define VOLUME 70

#define CLK_PIN 25 // ESP32 pin GPIO25 connected to the rotary encoder's CLK pin
#define DT_PIN 26  // ESP32 pin GPIO26 connected to the rotary encoder's DT pin
#define SW_PIN 27  // ESP32 pin GPIO27 connected to the rotary encoder's SW pin

#define DIRECTION_CW 0  // clockwise direction
#define DIRECTION_CCW 1 // counter-clockwise direction

#define NEXT_BUTTON_PIN 32 // ESP32 pin GPIO18, which connected to button

int counter_volume = 70;
int stream_counter = 0;
int max_volume = 100;
int min_volume = 0;
int direction = DIRECTION_CW;
int CLK_state;
int prev_CLK_state;
int next_button_state = 0; // variable for reading the button status

const char *current_url;

ezButton button(SW_PIN); // create ezButton object that attach to pin 27;
ezButton next_button(NEXT_BUTTON_PIN);

TaskHandle_t Task1;

UrlManager urlManager("myApp");

PrioWebServer webServer(80);

Play player(VS1053_CS, VS1053_DCS, VS1053_DREQ);




void setup()
{
  Serial.begin(115200);

  xTaskCreatePinnedToCore(
      Task1code, // Function to implement the task
      "Task1",   // Name of the task
      1000,      // Stack size in bytes
      NULL,      // Task input parameter
      0,         // Priority of the task
      &Task1,    // Task handle.
      0          // Core where the task should run
  );

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

  // Wait for VS1053 and PAM8403 to power up
  // otherwise the system might not start up correctly
  delay(3000);

  // This can be set in the IDE no need for ext library
  // system_update_cpu_freq(160);

  // Gebruik de klasse in de main

  urlManager.addUrl("https://icecast.omroep.nl/radio1-bb-mp3:443");
  urlManager.addUrl("https://icecast.omroep.nl/radio2-bb-mp3:443");
  urlManager.addUrl("hhttps://icecast.omroep.nl/3fm-bb-mp3:443");

  Serial.println("\n\nSimple Radio Node WiFi Radio");

  SPI.begin();

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

    current_url = urlManager.getUrlAtIndex(stream_counter);
    Serial.println("Huidige url");
    Serial.println(current_url);

    player.begin();
    player.switchToMp3Mode();
    player.setVolume(VOLUME);
    webServer.begin();
  }
}

void loop()
{

  // rotary encoder
  button.loop(); // MUST call the loop() function first
  next_button.loop();


  // read the current state of the rotary encoder's CLK pin
  CLK_state = digitalRead(CLK_PIN);

  // If the state of CLK is changed, then pulse occurred
  // React to only the rising edge (from LOW to HIGH) to avoid double count
  if (CLK_state != prev_CLK_state && CLK_state == HIGH)
  {
    // if the DT state is HIGH
    // the encoder is rotating in counter-clockwise direction => decrease the counter
    if (digitalRead(DT_PIN) == HIGH)
    {
      if (counter_volume > 0)
      {
        counter_volume--;
      }
      direction = DIRECTION_CCW;
    }
    else
    {
      // the encoder is rotating in clockwise direction => increase the counter
      if (counter_volume < 100)
      {
        counter_volume++;
      }
      direction = DIRECTION_CW;
    }

    Serial.print("Rotary Encoder:: direction: ");
    if (direction == DIRECTION_CW)
      Serial.print("Clockwise");
    else
      Serial.print("Counter-clockwise");

    Serial.print(" - count: ");
    Serial.println(counter_volume);
    player.setVolume(counter_volume);
  }

  // save last CLK state
  prev_CLK_state = CLK_state;

  if (button.isPressed())
  {
    Serial.println("The button is pressed");
  }

  next_button_state = next_button.getState();

  if (next_button.isPressed())
  {
    Serial.println("The button is pressed");
    if (stream_counter == 2)
    {
      stream_counter = 0;
    }
    else
    {
      stream_counter++;
    }
    current_url = urlManager.getUrlAtIndex(stream_counter);
    Serial.println("playing:");
    Serial.println(current_url);
    // player.stop();
    // player.resetStream();
  }

  if (next_button.isReleased())
    Serial.println("The button is released");

//  player.play(current_url);
    Serial.println("Playing:");
    Serial.println(urlManager.getUrlAtIndex(stream_counter));
}

void Task1code(void *pvParameters)
{
  while (1)
  {
    // Serial.print("Hello");
    delay(500); // wait for half a second
    // Serial.print(" World from loop2() at ");
    // Serial.println(xPortGetCoreID());
    // delay(500); // wait for half a second
  }
}
