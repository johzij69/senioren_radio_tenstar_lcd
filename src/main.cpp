// inspired by https://github.com/schreibfaul1/ESP32-MiniWebRadio/tree/master

#include "main.h"
#include "WiFi.h"
#include <WiFiManager.h>
#include <AsyncTCP.h>
#include "PrioWebserver.h"
#include "PrioBar.h"
#include "StreamLogo.h"
#include "SPI.h"
#include <TFT_eSPI.h>
#include "Free_Fonts.h"
#include "middleFont.h"
#include "vs1053_ext.h"
#include <ezButton.h> // the library to use for SW pin
#include "MyPreferences.h"
#include "UrlManager.h"
#include "PrioRotary.h"
#include "smallFont.h"  // bar
#include "middleFont.h" // bar
#include <SPIFFS.h>     // Include the SPIFFS library

#include <HTTPClient.h> // Include the HTTPClient library
#include "fetchImage.h"

#include <ESP32Time.h>
// Include the jpeg decoder library

#include "arrow.h"
#include "city.h"

#include "PrioScrollText.h"

/* touch */
#define TOUCH_SDA 21
#define TOUCH_SCL 22
#define TOUCH_WIDTH 480
#define TOUCH_HEIGHT 320

/* DAC */
#define VS1053_MOSI 23
#define VS1053_MISO 19
#define VS1053_SCK 18
#define VS1053_CS 5
#define VS1053_DCS 16
#define VS1053_DREQ 4

/* Rotary Volume */
#define CLK_PIN 25      // ESP32 pin GPIO25 connected to the rotary encoder's CLK pin
#define DT_PIN 26       // ESP32 pin GPIO26 connected to the rotary encoder's DT pin
#define SW_PIN 27       // ESP32 pin GPIO27 connected to the rotary encoder's SW pin
#define DIRECTION_CW 0  // clockwise direction
#define DIRECTION_CCW 1 // counter-clockwise direction

#define VOLUME_STEPS 30
#define MIN_VOLUME 0
#define DEF_VOLUME 20

/* Next Button */
#define NEXT_BUTTON_PIN 33 // ESP32 pin GPIO33, which connected to button

#define bck TFT_BLACK


ESP32Time rtc(0);

TFT_eSPI tft = TFT_eSPI();


int x = 20;
PRIO_GT911 touchp = PRIO_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_WIDTH, TOUCH_HEIGHT);

int title_lenght;
int station_lenght;
int tcount = 0;
int ani = 0;

int stream_index = 0;
int next_button_state = 0; // variable for reading the button status
int last_volume;
int max_volume = 25; // Set default to 25.

ezButton rotary_button(SW_PIN); // create ezButton object that attach to pin 27;
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

String last_url;
String current_title;

const char *current_url;

bool next_button_isreleased = true;

PrioBar pBar(&tft);

StreamLogo strLogo(&tft);

ScrollText *scrollText;
ScrollText *scrollText2;
ScrollText *scrollText3;

// Interrupt routine just sets a flag when rotation is detected
void IRAM_ATTR checkVolume()
{
  rotaryInstance.rotaryEncoder = true;
}

void setup()
{
  Serial.begin(115200);
  // while (!Serial)
  //   ;

  SPI.begin(VS1053_SCK, VS1053_MISO, VS1053_MOSI);

  /* start touch */
  touchp.begin();
  touchp.setRotation(ROTATION_LEFT);

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
  /* Rotary button */
  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN, INPUT);
  rotary_button.setDebounceTime(50); // set debounce time to 50 milliseconds

  /* Next Button */
  pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
  next_button.setDebounceTime(150);
  next_button_state = next_button.getState();

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
    /* get last used stream*/
    stream_index = myPrefs.getUInt("stream_index", 0);
    last_url = UrlManagerInstance.Streams[stream_index].url;
    current_url = last_url.c_str();

    Serial.print("Last url: ");
    Serial.println(last_url);
    Serial.print("Huidige url: ");
    Serial.println(current_url);
    Serial.print("Aantal urls: ");
    Serial.println(UrlManagerInstance.streamCount);

    Serial.println("mp3 begin");
    mp3.begin();

    Serial.println("set volume steps");
    mp3.setVolumeSteps(VOLUME_STEPS);
    max_volume = mp3.maxVolume();

    last_volume = myPrefs.readValue("volume", DEF_VOLUME);
    if (last_volume > max_volume)
    {
      last_volume = max_volume;
    }
    mp3.setVolume(last_volume);
    Serial.println("conecting");
    if (last_url != "")
    {
 //     mp3.connecttohost(last_url.c_str());
    }
    else
    {
//      mp3.connecttohost("https://stream.100p.nl/100pctnl.mp3");
    }
    Serial.println("conected");

    attachInterrupt(digitalPinToInterrupt(CLK_PIN), checkVolume, CHANGE);
    attachInterrupt(digitalPinToInterrupt(DT_PIN), checkVolume, CHANGE);
    rotaryInstance.begin(MIN_VOLUME, max_volume, DEF_VOLUME);
    rotaryInstance.current_value = last_volume;

    /* sceen output */
    Serial.println("Initializing TFT...");
    tft.init();
    tft.setRotation(1); // Landscape mode
    tft.fillScreen(TFT_BLACK);
    //tft.setSwapBytes(true);

    Serial.println("TFT initialized");

    // Optioneel: stel kleuren in
    scrollText = new ScrollText(tft, "Hier is de nieuwe tekst die moet gaan scrollen op het tft scherm", 50, 20, 400, 40, FSS12);
    scrollText->setTextBetweenSpace(20);
    scrollText->setScrollSpeed(5);
    scrollText->setScrollDelay(5000);
    scrollText->setScrollDirection(ScrollDirection::LEFT_TO_RIGHT_TO_LEFT);
    scrollText->begin();
         scrollText->setText("Dit is de nieuwe tekst die is aangepast, geen idee of dit langgenoeg is.");


    scrollText2 = new ScrollText(tft, "Dit is een lange tekst die zal scrollen op het tft scherm", 20, 50, 300, 40, FSS12);
     scrollText2->setScrollSpeed(3);
    scrollText2->setScrollDelay(2000);
    scrollText2->setScrollDirection(ScrollDirection::RIGHT_TO_LEFT_ONCE);
    scrollText2->begin();
    

    scrollText3 = new ScrollText(tft, "Dit is een lange tekst die zal scrollen op het tft scherm", 20, 80, 300, 40, FSS12);
    scrollText3->setTextBetweenSpace(20);
    scrollText3->setScrollDelay(5000);
    scrollText3->setScrollDirection(ScrollDirection::RIGHT_TO_LEFT_CONT);
    scrollText3->setScrollSpeed(2);
    scrollText3->begin();


  }

  pBar.begin(450, 0, 30, 320, TFT_LIGHTGREY, TFT_BLACK, max_volume);
  pBar.draw(last_volume);

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
  webServer.begin();
}
void loop()
{
   scrollText->update();



  // scrollText2->update();
  //  scrollText3->update();
//  getTouch(touchp);
}

void mydraw()
{

  //    ani--;
  //   if (ani < -420)
  //     ani = 10;

  //   sBackground.fillSprite(TFT_BLACK);
  //   sTitel_text.fillSprite(TFT_BLACK);
  //   sTitel_text.drawString(String(x), 0, 0,4);
  //  arrowSprite.pushImage(0,0,96,96,arrow);
  //  arrowSprite.pushToSprite(&sBackground,x,5,TFT_BLACK);

  //   sTitel_text.pushToSprite(&sBackground,ani,10,TFT_BLACK);

  //   // sTitel_text.pushSprite(0,50);
  //   sBackground.pushSprite(0, 0);
  //    x++;

  //  if(x>450)
  //  x=-100;
}

void oldloop()
{
  mp3.loop();
  rotaryInstance.loop();
  next_button.loop();
  rotary_button.loop();
  if (rotaryInstance.current_value_changed)
  {
    pBar.draw(rotaryInstance.current_value);
    mp3.setVolume(rotaryInstance.ReadRotaryValue());
    myPrefs.writeValue("volume", rotaryInstance.current_value);
  }

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

    mp3.connecttohost(current_url);
    strLogo.Show(35, 100, UrlManagerInstance.Streams[stream_index].logo);
  }

  if (next_button.isReleased())
  {
    /* we only will listen to button input if it was released */
    next_button_isreleased = true;
    Serial.println("The button is released");
    Serial.println(current_url);
  }

  if (current_title == "")
  {
    current_title = "Titel is onbekend.";
  }

  getTouch(touchp);
}

void getTouch(PRIO_GT911 &tp)
{
  tp.read();
  if (tp.isTouched)
  {
    for (int i = 0; i < tp.touches; i++)
    {
      Serial.print("Touch ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print("  x: ");
      Serial.print(tp.points[i].x);
      Serial.print("  y: ");
      Serial.print(tp.points[i].y);
      Serial.print("  size: ");
      Serial.println(tp.points[i].size);
      Serial.println(' ');
    }
  }
}

String readDefaultUrl()
{

  String defaultUrl = "https://icecast.omroep.nl/radio1-bb-mp3:443";
  String result = myPrefs.readString("lasturl", defaultUrl.c_str());
  return result;
}

void setTitle(const char *title)
{
  Serial.print("before:");
  Serial.println(title);
  current_title = title;
  // Serial.print("current_title:");
  // Serial.println(current_title);
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
  // tft.fillRect(0, 50, 450, 17, TFT_BLACK);
  // tft.setCursor(0, 50);
  // tft.setFreeFont(FSB9);
  // tft.setTextColor(TFT_WHITE, TFT_BLACK);
  // tft.drawString(info, 0, 50);
}
void vs1053_showstreamtitle(const char *info)
{ // called from vs1053

  // see -> https://github.com/RalphBacon/205-Internet-Radio/blob/main/PlatformIO/ESP32%20Better%20Internet%20Radio%20-%20YouTube/include/tftHelpers.h
  Serial.print("STREAMTITLE:  ");
  Serial.println(info); // Show title
  tft.fillRect(0, 75, 450, 17, TFT_BLACK);
  tft.setCursor(0, 75);
  tft.setFreeFont(FSB9);
  // tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  // tft.println(info); // Show title
  tft.drawString(info, 0, 75);
  setTitle(info);
}
void vs1053_showstreaminfo(const char *info)
{ // called from vs1053
  Serial.print("STREAMINFO:   ");
  Serial.println(info); // Show streaminfo
  // tft.println(info); // Show title
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
