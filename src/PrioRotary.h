#ifndef PrioRotary_h
#define PrioRotary_h




class PrioRotary
{

public:
    PrioRotary(int pin1, int pin2);
    // ~PrioRotary();

    void loop();
    void begin(int _min_value, int _max_value, int _default_value);
    void begin(int _min_value, int _max_value);
    int ReadRotaryValue();

    volatile bool rotaryEncoder;
    bool current_value_changed;

    int rotationCounter;
    int max_value;
    int min_value;
    int current_value;
    int default_value;

    // Functiepointer voor de dummy_function
    //   void (*useValuePointer)(uint8_t);

private:
    int _pin1, _pin2;
    int8_t checkRotaryEncoder();
};

#endif // ANOTHER_CLASS_H