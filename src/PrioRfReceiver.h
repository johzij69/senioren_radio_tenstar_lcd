#ifndef PRIO_RF_RECEIVER_H
#define PRIO_RF_RECEIVER_H

#include <Arduino.h>
#include <RCSwitch.h>

typedef void (*KeyReceivedCallback)(int key);

class PrioRfReceiver
{
public:
    PrioRfReceiver(int rxPin);
    void begin();
    void loop();
    void setKeyReceivedCallback(KeyReceivedCallback callback);

private:
    RCSwitch mySwitch;
    int rxPin;
    KeyReceivedCallback keyReceivedCallback;

    int getReceivedKey(unsigned long receivedValue);
    void decodeRawData(unsigned int *rawData, int length);
};

#endif