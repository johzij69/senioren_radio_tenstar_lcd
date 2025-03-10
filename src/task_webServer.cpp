#include "task_webServer.h"
#include "task_shared.h"

void webServerTask(void *parameter)
{
    // Cast the parameter back to a PrioWebServer pointer
    PrioWebServer* webserver = static_cast<PrioWebServer*>(parameter);

    // Use the webserver object
    webserver->ip = WiFi.localIP().toString();
    webserver->begin();
    xEventGroupSetBits(taskEvents, WEBSERVER_TASK_STARTED_BIT);
    // Infinite loop for the task
    while (true)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS); // Adjust the delay as needed
    }
}
