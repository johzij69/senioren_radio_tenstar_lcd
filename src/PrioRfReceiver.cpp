#include "PrioRfReceiver.h"

PrioRfReceiver::PrioRfReceiver(int rxPin) : rxPin(rxPin), keyReceivedCallback(nullptr) {}

void PrioRfReceiver::begin()
{
    Serial.begin(115200);
    mySwitch.enableReceive(digitalPinToInterrupt(rxPin));
    Serial.println("RF-ontvanger is gereed om signalen te ontvangen...");
}

void PrioRfReceiver::loop()
{
    if (mySwitch.available())
    {
        unsigned long receivedValue = mySwitch.getReceivedValue();
        int key = getReceivedKey(receivedValue);

        if (keyReceivedCallback != nullptr && key != 0)
        {
            keyReceivedCallback(key-1); // Roep de callback-functie aan
        }

        unsigned int bitLength = mySwitch.getReceivedBitlength();
        unsigned int protocol = mySwitch.getReceivedProtocol();
        unsigned int delay = mySwitch.getReceivedDelay();
        unsigned int *rawData = mySwitch.getReceivedRawdata();
        int rawDataLength = mySwitch.getReceivedBitlength() * 2;

        decodeRawData(rawData, rawDataLength);

        Serial.print("Ontvangen waarde: ");
        Serial.print(receivedValue);
        Serial.print(" | Bitlengte: ");
        Serial.print(bitLength);
        Serial.print(" | Protocol: ");
        Serial.print(protocol);
        Serial.print(" | Pulslengte: ");
        Serial.println(delay);

        Serial.println("Ruwe data (pulslengtes in µs):");
        for (int i = 0; i < rawDataLength; i++)
        {
            Serial.print(rawData[i]);
            Serial.print(" ");
        }
        Serial.println();

        mySwitch.resetAvailable();
    }
}

void PrioRfReceiver::setKeyReceivedCallback(KeyReceivedCallback callback)
{
    keyReceivedCallback = callback;
}

int PrioRfReceiver::getReceivedKey(unsigned long receivedValue)
{
    switch (receivedValue)
    {
    case 12496872:
        return 1;
    case 12496868:
        return 2;
    case 12496876:
        return 3;
    case 12496866:
        return 4;
    case 12496874:
        return 5;
    case 12496870:
        return 6;
    case 12496878:
        return 7;
    case 12496865:
        return 8;
    case 12496873:
        return 9;
    case 12496869:
        return 10;
    default:
        return 0;
    }
}

void PrioRfReceiver::decodeRawData(unsigned int *rawData, int length)
{
    for (int i = 0; i < length; i++)
    {
        if (rawData[i] > 800)
        { // Lange puls (bijvoorbeeld ~900 µs)
            Serial.print("1 ");
        }
        else if (rawData[i] > 200 && rawData[i] < 400)
        { // Korte puls (bijvoorbeeld ~300 µs)
            Serial.print("0 ");
        }
        else
        {
            Serial.print("? "); // Onbekende puls
        }
    }
    Serial.println();
}