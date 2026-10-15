#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class I2cBusGuard
{
public:
    I2cBusGuard()
        : semaphore(sharedMutex()), acquired(semaphore && xSemaphoreTake(semaphore, pdMS_TO_TICKS(100)) == pdTRUE)
    {
    }

    ~I2cBusGuard()
    {
        if (acquired) xSemaphoreGive(semaphore);
    }

    explicit operator bool() const { return acquired; }
    I2cBusGuard(const I2cBusGuard &) = delete;
    I2cBusGuard &operator=(const I2cBusGuard &) = delete;

private:
    static SemaphoreHandle_t sharedMutex()
    {
        static SemaphoreHandle_t semaphore = xSemaphoreCreateMutex();
        return semaphore;
    }

    SemaphoreHandle_t semaphore;
    bool acquired;
};