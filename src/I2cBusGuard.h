#pragma once

#include <Arduino.h>
#include <Wire.h>
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

    bool readBytes(uint8_t address, uint8_t *data, size_t size, const char *device)
    {
        if (!acquired) return false;
        size_t received = Wire.requestFrom(address, size);
        if (received != size || Wire.available() < static_cast<int>(size))
        {
            log_e("[I2C] %s address=0x%02X read failed: received=%u expected=%u",
                device, address, static_cast<unsigned>(received), static_cast<unsigned>(size));
            return false;
        }
        for (size_t index = 0; index < size; ++index) data[index] = Wire.read();
        return true;
    }

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