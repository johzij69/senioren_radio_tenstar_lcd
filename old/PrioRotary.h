#ifndef PrioRotary_h
#define PrioRotary_h
#include "Arduino.h"

class PrioRotary
{

public:
    PrioRotary();
    ~PrioRotary();
    volatile bool rotaryEncoder;
    int rotationCounter;

    // Functiepointer voor de dummy_function
    void (*useValuePointer)(int);

    int8_t checkRotaryEncoder();
}

#endif // ANOTHER_CLASS_H