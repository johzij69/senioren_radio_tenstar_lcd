#include "Task_Display.h"
#include "PrioTft.h"
#include "Task_Shared.h"

int huidigePWN = 100;                             // beginwaarde
const ledc_channel_t pwmChannel = LEDC_CHANNEL_0; // Kanaal 0-7
float previousLux = 0.0;

// Drempelinstelling in lux
const float LUX_THRESHOLD = 0.5;

// Fade-instelling (hoe snel hij aanpast)
const int FADE_STEP = 2;

bool fixedbacklight = false; // Zet deze op true om de backlight op een vaste waarde te zetten

void DisplayTask(void *parameter)
{
    Serial.println("Display task started");

    PrioTft prioTft;

    prioTft.begin(); // Initialiseer het TFT scherm
    if (!prioTft.isInitialized)
    {
        Serial.println("TFT-initialisatie mislukt. Controleer hardwareverbindingen.");
    }
    //  setup_backlight(); // Initialiseer de backlight
    Adafruit_VEML7700 veml = Adafruit_VEML7700();
    Serial.println("Display task started3");
    bool sensorFound = false;
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        if (veml.begin())
        {
            sensorFound = true;
            break;
        }
        Serial.println("Sensor not found, retrying...");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
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

    static unsigned long lastBacklightUpdate = 0;

    QueueHandle_t DisplayQueue = static_cast<QueueHandle_t>(parameter);

    // Variabelen om de vorige waarden op te slaan
    int prevVolume = -1;
    String prevIp = "";
    String prevTitle = "";
    String prevLogo = "";
    String prevStreamTitle = "";
    String prevTime = "";

    // Zet het event om aan te geven dat de DisplayTask is gestart
    xEventGroupSetBits(taskEvents, DISPLAY_TASK_STARTED_BIT);
    while (true)
    {

        DisplayData _displayData;
        if (xQueueReceive(DisplayQueue, &_displayData, 0) == pdTRUE)
        {

            spl("Data redceived for display task");
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

            Serial.println("Display: streamtitle" + String(_displayData.streamtitle));

            if (prevVolume != _displayData.volume)
            {
                prioTft.setVolume(_displayData.volume);
                prevVolume = _displayData.volume;
            }
            if (prevLogo != _displayData.logo)
            {
                prioTft.setLogo(_displayData.logo);
                prevLogo = _displayData.logo;
            }
        }

        //      Serial.println("Display: currenTime" + String(_displayData.currenTime));
        if (prevTime != _displayData.currenTime)
        {
            prioTft.showTime(_displayData.currenTime);
            prevTime = _displayData.currenTime;
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
        //        AdjustBackLight(veml);                // Pas de helderheid aan op basis van de omgevingslichtsensor
        vTaskDelay(100 / portTICK_PERIOD_MS); // Adjust the delay as needed (e.g., 10ms)
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
