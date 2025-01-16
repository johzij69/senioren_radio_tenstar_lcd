#include "task_webServer.h"

void webServerTask(void *parameter)
{
    // Cast the parameter back to a PrioWebServer pointer
    PrioWebServer* webserver = static_cast<PrioWebServer*>(parameter);

    // Use the webserver object
    webserver->ip = WiFi.localIP().toString();
    webserver->begin();

    // Infinite loop for the task
    while (true)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS); // Adjust the delay as needed
    }
}
