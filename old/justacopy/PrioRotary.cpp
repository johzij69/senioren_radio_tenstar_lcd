#include "Arduino.h"
#include "PrioRotary.h"

#define DEF_VOLUME 20
PrioRotary::PrioRotary(int pin1, int pin2)
{
    _pin1 = pin1;
    _pin2 = pin2;
    pinMode(pin1, INPUT);
    pinMode(pin2, INPUT);
}

void PrioRotary::begin(int _min_value, int _max_value)
{
    this->begin(_min_value, _max_value, DEF_VOLUME);
}
void PrioRotary::begin(int _min_value, int _max_value, int _default_value)
{
    rotaryEncoder = false;
    current_value_changed = false;
    max_value = _max_value;
    min_value = _min_value;
    current_value = _default_value;
    default_value = _default_value;
}

// Rotary encoder has moved (interrupt tells us) but what happened?
// See https://www.pinteric.com/rotary.html
int8_t PrioRotary::checkRotaryEncoder()
{
    // Reset the flag that brought us here (from ISR)
    rotaryEncoder = false;

    static uint8_t lrmem = 3;
    static int lrsum = 0;
    static int8_t TRANS[] = {0, -1, 1, 14, 1, 0, 14, -1, -1, 14, 0, 1, 14, 1, -1, 0};

    // Read BOTH pin states to deterimine validity of rotation (ie not just switch bounce)
    int8_t l = digitalRead(_pin1);
    int8_t r = digitalRead(_pin2);

    // Move previous value 2 bits to the left and add in our new values
    lrmem = ((lrmem & 0x03) << 2) + 2 * l + r;

    // Convert the bit pattern to a movement indicator (14 = impossible, ie switch bounce)
    lrsum += TRANS[lrmem];

    /* encoder not in the neutral (detent) state */
    if (lrsum % 4 != 0)
    {
        return 0;
    }

    /* encoder in the neutral state - clockwise rotation*/
    if (lrsum == 4)
    {
        lrsum = 0;
        return -1;
    }

    /* encoder in the neutral state - anti-clockwise rotation*/
    if (lrsum == -4)
    {
        lrsum = 0;
        return 1;
    }

    // An impossible rotation has been detected - ignore the movement
    lrsum = 0;
    return 0;
}

void PrioRotary::loop()
{
    if (rotaryEncoder)
    {
        // Get the movement (if valid)
        int8_t rotationValue = checkRotaryEncoder();

        // If valid movement, do something
        if (rotationValue != 0)
        {
            //  Serial.println(String(rotationValue));
            if (current_value > min_value && current_value < max_value || current_value == max_value && rotationValue == -1 || current_value == min_value && rotationValue == 1)
            {
                rotationValue < 1 ? current_value-- : current_value++;
                current_value_changed = true;
            }
            Serial.println(String(current_value));
        }
    }
}
int PrioRotary::ReadRotaryValue()
{
    current_value_changed = false;
    return this->current_value;
}
