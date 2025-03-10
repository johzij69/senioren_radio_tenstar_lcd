#ifndef TASK_SHARED_H
#define TASK_SHARED_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

// Definieer de events
#define DISPLAY_TASK_STARTED_BIT (1 << 0)
#define WEBSERVER_TASK_STARTED_BIT (1 << 1)

// Declareer de event group als extern
extern EventGroupHandle_t taskEvents;

#endif