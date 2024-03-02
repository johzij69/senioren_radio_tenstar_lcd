#ifndef PrioRotary_h
#define PrioRotary_h

class PrioRotary
{

public:
    PrioRotary(int pin1, int pin2);
    // ~PrioRotary();

    void loop();
    void begin();
    int ReadRotaryValue();

    volatile bool rotaryEncoder;
    bool rotary_value_changed;
    int rotationCounter;
    int rotary_value;

    // Functiepointer voor de dummy_function
    //   void (*useValuePointer)(uint8_t);

private:
    int _pin1, _pin2;
    int8_t checkRotaryEncoder();
};

#endif // ANOTHER_CLASS_H