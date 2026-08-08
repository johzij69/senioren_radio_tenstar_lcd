#include "Task_Display.h"
#include "Task_Shared.h"

PrioTft prioTft;
bool isMenuActive = false;
bool fixedbacklight = false; // Zet deze op true om de backlight op een vaste waarde te zetten

int last_volume_for_menu = 0; // Initialize with your default volume
int last_volume = 10;

PrioRotaryMenu myMenu(prioTft.tft); // Create a RotaryMenu instance using the TFT object from PrioTft
PrioDateTime pDateTime(RTC_CLK_PIN, RTC_DAT_PIN, RTC_RST_PIN);
PrioRotary rotaryInstance(ROT_CLK_PIN, ROT_DT_PIN);

/* Buttons */
ezButton rotary_button(ROT_SW_PIN);

int huidigePWN = 100;                             // beginwaarde
const ledc_channel_t pwmChannel = LEDC_CHANNEL_0; // Kanaal 0-7

float previousLux = 0.0;
// Drempelinstelling in lux
const float LUX_THRESHOLD = 0.5;
// Fade-instelling (hoe snel hij aanpast)
const int FADE_STEP = 2;

const char* menuJson = R"(
[
  {
    "label": "Alarm",
    "items": [
      { "label": "Set Alarm", "action": "setAlarm" },
      { "label": "Alarm On/Off", "action": "toggleAlarm" }
    ]
  },
  {
    "label": "Time",
    "items": [
      { "label": "24h", "action": "set24hMode" },
      { "label": "Daylight Savings", "action": "setDST" },
      { "label": "Sync Time", "action": "syncTime" }
    ]
  },
  {
    "label": "Other",
    "items": [
      { "label": "Stop Webserver", "action": "stopWebserver" }
    ]
  }
]
)";
void DisplayTask(void *parameter)
{
    Serial.println("Display task starting...");
    int max_volume = MAX_VOLUME; // set default max volume

    Adafruit_VEML7700 veml = Adafruit_VEML7700();
    MyPreferences myPrefs("myRadio");

    bool sensorFound = false;
    bool inStandby = false; // Flag to indicate if the system is in standby mode
    bool fromStandby = false; // Flag to indicate if the system is coming from standby

    static unsigned long lastBacklightUpdate = 0;

    QueueHandle_t DisplayQueue = static_cast<QueueHandle_t>(parameter);

    // Variabelen om de vorige waarden op te slaan
    int prevVolume = -1;
    String prevIp = "";
    String prevTitle = "";
    String prevLogo = "";
    String prevStreamTitle = "";
    String prevAlarmState = "";
    String prevTime = "";


    /* Rotary button */
    rotary_button.setDebounceTime(50); // set debounce time to 50 milliseconds

    /* Rotary Wheel */
    attachInterrupt(digitalPinToInterrupt(ROT_CLK_PIN), checkVolume, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ROT_DT_PIN), checkVolume, CHANGE);

    rotaryInstance.begin(MIN_VOLUME, max_volume, DEF_VOLUME);
    
    
    //last_volume_for_menu = last_volume; // Sync initial volume
    /* Volume handling */
    last_volume = myPrefs.readValue("volume", DEF_VOLUME);
    if (last_volume > max_volume)
    {
        last_volume = max_volume;
    }

    rotaryInstance.current_value = last_volume;
    Serial.println("DisplayTask: last_volume: " + String(last_volume));

    prioTft.begin(); // Initialiseer het TFT scherm
    Serial.println("DisplayTask: TFT scherm is geïnitialiseerd");
    if (!prioTft.isInitialized)
    {
        Serial.println("TFT-initialisatie mislukt. Controleer hardwareverbindingen.");
    }

    Serial.println("DisplayTask: TFT scherm is klaar");

   // Initialize Menu
    myMenu.setCallbacks(onMenuAction, onMenuOpen, onMenuClose);
    myMenu.loadMenu(menuJson);
    

    for (int attempt = 0; attempt < 5; ++attempt)
    {
        Serial.print("DisplayTask: Initializing light sensor, attempt ");
        Serial.println(attempt + 1);
        if (veml.begin())
        {
            sensorFound = true;
            break;
        }
        Serial.println("Sensor not found, retrying...");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    //  sensorFound = false; // Probeer de sensor te initialiseren
    if (!sensorFound)
    {
        Serial.println("Light sensor not found after 5 attempts, continuing without sensor.");
    }
    else
    {
        Serial.println("Light Sensor found");
        veml.setGain(VEML7700_GAIN_1);
        veml.setIntegrationTime(VEML7700_IT_100MS);
    }
    setup_backlight(); // Initialiseer de backlight

    // Zet het event om aan te geven dat de DisplayTask is gestart

    Serial.println("DisplayTask: zetten van DISPLAY_TASK_STARTED_BIT");
    xEventGroupSetBits(taskEvents, DISPLAY_TASK_STARTED_BIT);
    Serial.println("DisplayTask: bit gezet");
    while (true)
    {
   
        DisplayData _displayData;
  
  
        // 1. BELANGRIJK: Update de button status voor debounce verwerking
        rotary_button.loop();

        // 2. Check of de knop net is ingedrukt
        if (rotary_button.isPressed()) 
        {
            Serial.println("Rotary button pressed! Toggling menu.");
            isMenuActive = !isMenuActive; // Toggle de menu status
            
            // Roep de callbacks handmatig aan, of laat de menu library dit doen
            if (isMenuActive) {
                onMenuOpen();
                // myMenu.show(); // Afhankelijk van je library, forceer een redraw
            } else {
                onMenuClose();
            }
        }

       rotaryInstance.loop();  

       if (rotaryInstance.current_value_changed)
        {
            
            
            if (isMenuActive) 
            {
                // --- MENU IS ACTIEF ---
                // Stuur de rotary waarde naar het menu voor navigatie
                // Let op: Kijk in de documentatie van PrioRotaryMenu hoe je dit exact doet.
                // Bijvoorbeeld: myMenu.handleRotation(rotaryInstance.current_value); 
                // of myMenu.navigate(rotaryInstance.current_value);
                
                Serial.println("Menu navigation triggered");
            }
            else 
            {
                // --- NORMALE MODE (VOLUME) ---
                Serial.println("Volume changed to: " + String(rotaryInstance.current_value));
                myPrefs.writeValue("volume", rotaryInstance.current_value);
                _displayData.volume = rotaryInstance.current_value;
    //           audioData.volume = rotaryInstance.current_value;
                last_volume = rotaryInstance.current_value;
                setAudioVolume(rotaryInstance.current_value);
                rotaryInstance.current_value_changed = false;
                prioTft.setVolume(_displayData.volume);
            }
            
            // Reset de flag ALTIJD, anders blijft hij in de if hangen
            rotaryInstance.current_value_changed = false; 

        }

         // 4. Menu Rendering (indien actief)
        if (isMenuActive) 
        {
            // De meeste menu libraries hebben een update, draw of render functie nodig
            // myMenu.update(); 
            // myMenu.draw();
        }
            
        // Optimalisatie: Gebruik de QueueReceive timeout in plaats van een vTaskDelay later
        // Hierdoor ontwaakt de task DIRECT als er data is, maar slaapt hij als er niets is.
        if (xQueueReceive(DisplayQueue, &_displayData, pdMS_TO_TICKS(10)) == pdTRUE)
   //   if (xQueueReceive(DisplayQueue, &_displayData, 0) == pdTRUE)
        {

            if (_displayData.loadingState)
            {
                prioTft.showLoadingState();
                Serial.println("Display: Loading state active");
            }
            else
            {
                if (_displayData.standbyState)
                {
                    inStandby = true; // Zet de standby status
                    fromStandby = true; // Zet de fromStandby status
                    prioTft.showStandbyTime(_displayData.currenTime, _displayData.currenDate);
                    Serial.println("Display: Standby state active");
                }
                else
                {

                    if(fromStandby){
                        prioTft.init();
                        fromStandby = false; // Reset fromStandby status
                        Serial.println("Display: Coming from standby, showing standby state");
                        prevIp = "";
                        prevTitle = "";
                        prevLogo = "";
                        prevStreamTitle = "";
                        prevAlarmState = "";
                        prevTime = "";
                        prevVolume = -1; // Reset previous values

                    }
               
                    spl("Data received for display task");
                    // spl("ip: " + String(_displayData.ip));
                    spl("tft_title: " + String(_displayData.title));
                    //           spl("volume: " + String(_displayData.volume));
                    // spl("logo: " + String(_displayData.logo));
                    // spl("bitrate: " + String(_displayData.bitrate));
                    spl("Tft_station: " + String(_displayData.station));
                    // spl("icyurl: " + String(_displayData.icyurl));
                    // spl("lasthost: " + String(_displayData.lasthost));
                    spl("tft_streamtitle: " + String(_displayData.streamtitle));
                    spl("currenTime: " + String(_displayData.currenTime));
                    spl("currenDate: " + String(_displayData.currenDate));

                    // Controleer en update alleen als de waarde is gewijzigd
                    if (prevIp != _displayData.ip)
                    {
                        prioTft.showLocalIp(_displayData.ip);
                        prevIp = _displayData.ip;
                    }
                    if (prevTitle != _displayData.title)
                    {
                        cleanStreamTitle(&_displayData); // When title is changed check if streamtitle needs to be cleaned
                        prioTft.setTitle(_displayData.title);
                        prevTitle = _displayData.title;
                    }
                    if (prevStreamTitle != _displayData.streamtitle)
                    {
                        cleanStreamTitle(&_displayData);
                        prioTft.setStreamTitle(_displayData.streamtitle);
                        prevStreamTitle = _displayData.streamtitle;
                    }

                    if (prevAlarmState != _displayData.alarmState)
                    {
                        prioTft.setAlarmState(_displayData.alarmState);
                        prevAlarmState = _displayData.alarmState;
                    }

                    Serial.println("Display: streamtitle" + String(_displayData.streamtitle));

                    if (prevLogo != _displayData.logo)
                    {
                        prioTft.setLogo(_displayData.logo);
                        prevLogo = _displayData.logo;
                    }

                    //      Serial.println("Display: currenTime" + String(_displayData.currenTime));
                       strncpy(_displayData.currenTime, pDateTime.getTime(), sizeof(_displayData.currenTime));
                   //    strncpy(_displayData.currenDate, pDateTime.getDayDate(), sizeof(_displayData.currenDate));   

                    if (prevTime != _displayData.currenTime)
                    {
                        prioTft.showTime(_displayData.currenTime);
                        prevTime = _displayData.currenTime;
                    }
                }
            }
        }

        if (sensorFound)
        {

            if (millis() - lastBacklightUpdate > 200) // 200 ms = 5x per seconde
            {
                AdjustBackLight(veml);
                lastBacklightUpdate = millis();
            }
        }
        else
        {
            if (!fixedbacklight)
            {
                fixedbacklight = true; // Zet de backlight op een vaste waarde
                // Als de sensor niet gevonden is, gebruik een standaard helderheid
                Serial.println("Light sensor not found, setting backlight to default brightness.");
                SetBacklightPWM(128); // Zet backlight op 50% als de sensor niet beschikbaar is
            }
        }

       // vTaskDelay(200 / portTICK_PERIOD_MS); // Adjust the delay as needed (e.g., 10ms) // is now handled by the xQueueReceive timeout
    }
}

void spl(String mes)
{
    Serial.println("Display:" + mes);
}

/*
In some cases the streamtitle wil start with the same text which is already in the title
We will remove this part from the streamtitle.
*/
void cleanStreamTitle(struct DisplayData *data)
{

    Serial.println("Display: stream title" + String(data->streamtitle));
    Serial.println("Display: title" + String(data->title));
    // Controleer of streamtitle begint met title
    if (strncmp(data->streamtitle, data->title, strlen(data->title)) == 0)
    {
        // Als dat zo is, verschuif alle karakters naar links
        size_t title_len = strlen(data->title);
        memmove(data->streamtitle,
                data->streamtitle + title_len,
                strlen(data->streamtitle) - title_len + 1); // +1 voor null-terminator
    }
}

void AdjustBackLight(Adafruit_VEML7700 veml)
{
    // Pas de helderheid aan op basis van de omgevingslichtsensor
    float lux = veml.readLux();

    // Serial.print("Lux: ");
    // Serial.println(lux);

    int brightness = mapLuxToPWM(lux);
    // Serial.print("Backlight brightness: ");
    // Serial.println(brightness);

    SetBacklightPWM(brightness);
}
void SetBacklightPWM(int brightness)
{
    // Zet de helderheid van de backlight in (0-255)
    ledc_set_duty(LEDC_LOW_SPEED_MODE, pwmChannel, brightness);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, pwmChannel);
}

void setup_backlight()
{

    const int backlightPin = BACKLIGHT_PIN;     // GPIO voor backlight
    const ledc_timer_t pwmTimer = LEDC_TIMER_0; // Timer 0-3 (nu wél gedefinieerd!)
    const int freq = 5000;                      // Frequentie (Hz)
    const int resolution = 8;

    // Stap 1: Timer configureren
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT, // Resolutie
        .timer_num = pwmTimer,               // Timer
        .freq_hz = freq,                     // Frequentie
        .clk_cfg = LEDC_AUTO_CLK             // Clock source
    };
    ledc_timer_config(&timer_cfg);

    // Stap 2: Kanaal koppelen aan GPIO
    ledc_channel_config_t channel_cfg = {
        .gpio_num = backlightPin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = pwmChannel,
        .timer_sel = pwmTimer,
        .duty = 0, // Start bij 0% duty cycle
        .hpoint = 0};
    ledc_channel_config(&channel_cfg);

    // Stap 3: We starten altijd met 50% helderheid
    ledc_set_duty(LEDC_LOW_SPEED_MODE, pwmChannel, 128);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, pwmChannel);
}

// Hulpfunctie: zet lux om naar PWM (0-255)
// Hulpfunctie: zet lux om naar PWM (0-255)
// int mapLuxToPWM(float lux)
// {
//     const float minLux = 1.0;
//     const float maxLux = 10000.0;
//     const int minPWM = 26;
//     const int maxPWM = 255;

//     // Lux afkappen binnen bereik
//     lux = constrain(lux, minLux, maxLux);

//     // Alleen aanpassen als verschil groot genoeg is
//     if (abs(lux - previousLux) < LUX_THRESHOLD)
//     {
//         return huidigePWN; // te kleine verandering → niets doen
//     }

//     // Logaritmische schaal
//     float logLux = log10(lux);
//     float logMin = log10(minLux);
//     float logMax = log10(maxLux);
//     float scaled = (logLux - logMin) / (logMax - logMin);
//     int targetPWM = round(minPWM + scaled * (maxPWM - minPWM));

//     // Fade toepassen: geleidelijke aanpassing
//     if (targetPWM > huidigePWN)
//     huidigePWN = min(huidigePWN + FADE_STEP, targetPWM);
//     else if (targetPWM < huidigePWN)
//     huidigePWN = max(huidigePWN - FADE_STEP, targetPWM);

//     previousLux = lux;
//     return huidigePWN;
// }

int mapLuxToPWM(float lux)
{
    // Minimale en maximale lux-waarden in jouw omgeving
    const float minLux = 20.0;   // Donkere kamer
    const float maxLux = 1000.0; // Zeer heldere ruimte / daglicht

    // Beperk lux tot binnen bereik
    if (lux < minLux)
        lux = minLux;
    if (lux > maxLux)
        lux = maxLux;

    // Inverse mapping: hoe meer licht, hoe minder backlight
    // Minder lux = meer helderheid, dus PWM = 255 bij minLux, 0 bij maxLux
    int targetPWM = map(lux, minLux, maxLux, 30, 255); // 30 als minimum brightness voor zichtbaarheid

    // Beperk PWM tot veilige grenzen
    if (targetPWM < 30)
        targetPWM = 30;
    if (targetPWM > 255)
        targetPWM = 255;

    if (targetPWM > huidigePWN)
        huidigePWN = min(huidigePWN + FADE_STEP, targetPWM);
    else if (targetPWM < huidigePWN)
        huidigePWN = max(huidigePWN - FADE_STEP, targetPWM);

    //   previousLux = lux;
    return huidigePWN;
}


// void sync_time(bool forceSync)
// {
//     if (forceSync)
//     {
//         pDateTime.syncTime();
//     }
//     else
//     {
//         pDateTime.checkSync();
//     }

//     strncpy(displayData.currenTime, pDateTime.getTime(), sizeof(displayData.currenTime));
//    // strncpy(displayData.currenDate, pDateTime.getDayDate(), sizeof(displayData.currenDate));
//     if (prevTime != displayData.currenTime)
//     {
//         prevTime = displayData.currenTime;
//         xQueueSend(DisplayQueue, &displayData, portMAX_DELAY);
//     }
// }

void onMenuAction(const char* action) {
    Serial.printf("Menu Action Triggered: %s\n", action);
    
    // Execute your system logic here
    // if (strcmp(action, "syncTime") == 0) {
    //     sync_time(true);
    // } else if (strcmp(action, "stopWebserver") == 0) {
    //     // stop webserver logic
    // }
}

void onMenuOpen() {
 //   isMenuActive = true;
    Serial.println("Menu Opened - Pausing Player UI updates");
}

void onMenuClose() {
  //  isMenuActive = false;
    Serial.println("Menu Closed - Redrawing Player UI");
    
    // Wipe screen completely and trigger your display task to redraw the player
    prioTft.tft.fillScreen(TFT_BLACK); 
    // CreateAndSendDisplayData(stream_index); 
}

// Interrupt routine just sets a flag when rotation is detected
void IRAM_ATTR checkVolume()
{
    rotaryInstance.rotaryEncoder = true;
}