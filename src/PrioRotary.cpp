#include "Arduino.h"
#include "PrioRotary.h"

#define DEF_VOLUME 20

PrioRotary::PrioRotary(int pin1, int pin2)
{
    _pin1 = pin1;
    _pin2 = pin2;
    pinMode(pin1, INPUT_PULLUP);
    pinMode(pin2, INPUT_PULLUP);
}

void PrioRotary::begin(int _min_value, int _max_value)
{
    this->begin(_min_value, _max_value, DEF_VOLUME);
}

void PrioRotary::begin(int _min_value, int _max_value, int _default_value)
{
    rotaryEncoder = false;
    current_value_changed = false;
    rotationCounter = 0;        // Reset de rauwe teller
    max_value = _max_value;
    min_value = _min_value;
    current_value = _default_value;
    default_value = _default_value;
}

int8_t PrioRotary::checkRotaryEncoder()
{
    rotaryEncoder = false;
    static uint8_t lrmem = 3;
    static int lrsum = 0;
    static int8_t TRANS[] = {0, -1, 1, 14, 1, 0, 14, -1, -1, 14, 0, 1, 14, 1, -1, 0};

    int8_t l = digitalRead(_pin1);
    int8_t r = digitalRead(_pin2);
    lrmem = ((lrmem & 0x03) << 2) + 2 * l + r;
    lrsum += TRANS[lrmem];

    if (lrsum % 4 != 0) return 0;
    if (lrsum == 4) { lrsum = 0; return -1; }
    if (lrsum == -4) { lrsum = 0; return 1; }
    lrsum = 0;
    return 0;
}

void PrioRotary::loop()
{
    if (rotaryEncoder)
    {
        int8_t rotationValue = checkRotaryEncoder();
        if (rotationValue != 0)
        {
            // NIEUW: altijd bijhouden voor menu navigatie (ongeacht volume clamping)
            rotationCounter += rotationValue;

            if (current_value > min_value && current_value < max_value || current_value == max_value && rotationValue == -1 || current_value == min_value && rotationValue == 1)
            {
                rotationValue < 1 ? current_value-- : current_value++;
                current_value_changed = true;
            }
        }
    }
}

int PrioRotary::ReadRotaryValue()
{
    current_value_changed = false;
    return this->current_value;
}

// NIEUW
int PrioRotary::getAndResetRotationCounter()
{
    int count = rotationCounter;
    rotationCounter = 0;
    return count;
}