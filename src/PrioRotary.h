#ifndef PrioRotary_h
#define PrioRotary_h

class PrioRotary
{
public:
    PrioRotary(int pin1, int pin2);
    void loop();
    void begin(int _min_value, int _max_value, int _default_value);
    void begin(int _min_value, int _max_value);
    int ReadRotaryValue();

    // NIEUW: geeft het aantal rotaties sinds laatste check en reset de teller
    int getAndResetRotationCounter();

    volatile bool rotaryEncoder;
    bool current_value_changed;

    int rotationCounter;   // Rauwe rotatie-teller (voor menu's)
    int max_value;
    int min_value;
    int current_value;
    int default_value;

private:
    int _pin1, _pin2;
    int8_t checkRotaryEncoder();
};

#endif