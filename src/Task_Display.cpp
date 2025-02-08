#include "Task_Display.h"
#include "PrioTft.h"

void DisplayTask(void *parameter)
{
    Serial.println("Display task started");

    PrioTft prioTft;
    prioTft.begin(); // Initialiseer het TFT scherm
    if (!prioTft.isInitialized)
    {
        Serial.println("TFT-initialisatie mislukt. Controleer hardwareverbindingen.");
    }

    QueueHandle_t DisplayQueue = static_cast<QueueHandle_t>(parameter);

    while (true)
    {

        DisplayData _displayData;
        if (xQueueReceive(DisplayQueue, &_displayData, 0) == pdTRUE)
        {

            spl("Data redceived for display task");
            spl("ip: " + String(_displayData.ip));
            spl("title: " + String(_displayData.title));
            spl("volume: " + _displayData.volume);
            spl("logo: " + String(_displayData.logo));

            prioTft.showLocalIp(_displayData.ip);
            prioTft.setTitle(_displayData.title);
            prioTft.setVolume(_displayData.volume);
            prioTft.setLogo(_displayData.logo);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // Adjust the delay as needed (e.g., 10ms)
    }
}

void spl(String mes)
{
    Serial.println("Display:"+mes);
}