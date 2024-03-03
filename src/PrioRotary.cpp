#include "Arduino.h"
#include "PrioRotary.h"

#define MAX_VOLUME 50
#define MIN_VOLUME 0
#define DEF_VOLUME 20

PrioRotary::PrioRotary(int pin1, int pin2)
{
    _pin1 = pin1;
    _pin2 = pin2;
    pinMode(pin1, INPUT);
    pinMode(pin2, INPUT);
}
// PrioRotary::~PrioRotary(){};

void PrioRotary::begin()
{
    rotary_value = DEF_VOLUME;
    rotaryEncoder = false;
    rotary_value_changed = false;
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
        return 1;
    }

    /* encoder in the neutral state - anti-clockwise rotation*/
    if (lrsum == -4)
    {
        lrsum = 0;
        return -1;
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
            if (rotary_value > MIN_VOLUME && rotary_value < MAX_VOLUME || rotary_value == MAX_VOLUME && rotationValue == -1 || rotary_value == MIN_VOLUME && rotationValue == 1)
            {
                rotationValue < 1 ? rotary_value-- : rotary_value++;
                rotary_value_changed = true;
            }
            Serial.println(String(rotary_value));
        }
    }
}
int PrioRotary::ReadRotaryValue() {
    rotary_value_changed = false;
    return this->rotary_value;

}
