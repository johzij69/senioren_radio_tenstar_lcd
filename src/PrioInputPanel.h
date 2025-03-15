#ifndef PRIOINPUTPANEL_H
#define PRIOINPUTPANEL_H

#include <Arduino.h>
#include <Wire.h>

class PrioInputPanel {
public:
  typedef void (*ButtonPressedCallback)(int buttonIndex); // Callback type

  PrioInputPanel(uint8_t address, uint8_t intPin, uint8_t sdaPin, uint8_t sclPin);
  void begin();
  void loop();
  void setButtonPressedCallback(ButtonPressedCallback callback); // Callback instellen

private:
  uint8_t _address;
  uint8_t _intPin;
  uint8_t _sdaPin;
  uint8_t _sclPin;

  volatile bool _keyPressed;
  unsigned long _lastDebounceTime;
  const unsigned long _debounceDelay = 10;

  ButtonPressedCallback _buttonPressedCallback; // Callback functie

  void readPresetButtons();
  void printBinary(int v, int num_places);
  static void keyPressedISR();

  static PrioInputPanel* _instance; // Voor ISR toegang tot de class
};

#endif