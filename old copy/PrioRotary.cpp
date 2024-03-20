#include "PrioRotary.h"

#define MAX_VOLUME 50
#define MIN_VOLUME 0
#define DEF_VOLUME 20

#define CLK_PIN 25 // ESP32 pin GPIO25 connected to the rotary encoder's CLK pin
#define DT_PIN 26  // ESP32 pin GPIO26 connected to the rotary encoder's DT pin

PrioRotary::PrioRotary(){};
PrioRotary::~PrioRotary(){};

PrioRotary::begin()
{
    attachInterrupt(digitalPinToInterrupt(CLK_PIN), rotary, CHANGE);
    attachInterrupt(digitalPinToInterrupt(DT_PIN), rotary, CHANGE);
    // Flag from interrupt routine (moved=true)
    rotaryEncoder = false;
}

// Interrupt routine just sets a flag when rotation is detected
PrioRotary::void IRAM_ATTR rotary()
{
    rotaryEncoder = true;
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
    int8_t l = digitalRead(CLK_PIN);
    int8_t r = digitalRead(DT_PIN);

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

PrioRotary::loop()
{
    if (rotaryEncoder)
    {
        // Get the movement (if valid)
        int8_t rotationValue = checkRotaryEncoder();

        // If valid movement, do something
        if (rotationValue != 0)
        {

            if (counter_volume > MIN_VOLUME && counter_volume < MAX_VOLUME || counter_volume == MAX_VOLUME && rotationValue == -1 || counter_volume == MIN_VOLUME && rotationValue == 1)
            {
                rotationValue < 1 ? counter_volume-- : counter_volume++;
            }
            // Gebruik de functiepointer om de functie vanuit de main aan te roepen
            if (useValuePointer)
            {
                useValuePointer(counter_volume);
            }
        }
    }
}
