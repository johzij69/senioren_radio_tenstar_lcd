#include "Task_Display.h"
#include "PrioTft.h"
#include "PrioDateTime.h"

void DisplayTask(void *parameter)
{
    Serial.println("Display task started");

    PrioTft prioTft;
    prioTft.begin(); // Initialiseer het TFT scherm
    if (!prioTft.isInitialized)
    {
        Serial.println("TFT-initialisatie mislukt. Controleer hardwareverbindingen.");
    }

    // Start de tijdservice
    Serial.println("Starting time service");
    PrioDateTime pDateTime;


    QueueHandle_t DisplayQueue = static_cast<QueueHandle_t>(parameter);

    // Variabelen om de vorige waarden op te slaan
    int prevVolume = -1;
    String prevIp = "";
    String prevTitle = "";
    String prevLogo = "";
    String prevStreamTitle = "";
    String prevTime="";


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

            Serial.println("Wat als leeg. Display: streamtitle" + String(_displayData.streamtitle));

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
        
        if(!pDateTime.timeSynced){
            pDateTime.begin();
        }
        strncpy(_displayData.currenTime, pDateTime.getTime(), sizeof(_displayData.currenTime));
        if(prevTime != _displayData.currenTime){
            prioTft.showTime(_displayData.currenTime);
            prevTime = _displayData.currenTime;
        }
        


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

        Serial.println("Kom i khier wel");
        // Als dat zo is, verschuif alle karakters naar links
        size_t title_len = strlen(data->title);
        memmove(data->streamtitle,
                data->streamtitle + title_len,
                strlen(data->streamtitle) - title_len + 1); // +1 voor null-terminator
    }
}