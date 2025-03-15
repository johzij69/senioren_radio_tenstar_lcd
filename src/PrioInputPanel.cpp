#include "PrioInputPanel.h"

// Initialiseer de static member
PrioInputPanel* PrioInputPanel::_instance = nullptr;

PrioInputPanel::PrioInputPanel(uint8_t address, uint8_t intPin, uint8_t sdaPin, uint8_t sclPin)
  : _address(address), _intPin(intPin), _sdaPin(sdaPin), _sclPin(sclPin), _keyPressed(false), _lastDebounceTime(0), _buttonPressedCallback(nullptr) {
  _instance = this; // Zet de static instance naar dit object
}

void PrioInputPanel::begin() {
  // I2C initialiseren
  Wire.begin(_sdaPin, _sclPin);

  // Interrupt pin configureren
  pinMode(_intPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(_intPin), keyPressedISR, FALLING);

  // PCF8575 configureren: alle pinnen als input
  Wire.beginTransmission(_address);
  Wire.write(0xFF); // Alle pinnen als input (1 = input)
  Wire.write(0xFF); // Tweede byte voor PCF8575 (16 pinnen)
  Wire.endTransmission();
}

void PrioInputPanel::loop() {
  if (_keyPressed) {
    _keyPressed = false;
    readPresetButtons();

    // Serial.println(String(_lastDebounceTime));
  }
 // delay(100);

}

void PrioInputPanel::setButtonPressedCallback(ButtonPressedCallback callback) {
  _buttonPressedCallback = callback; // Stel de callback in
}

void PrioInputPanel::readPresetButtons() {
  // Lees de status van de PCF8575
  Wire.beginTransmission(_address);
  if (Wire.requestFrom(_address, 2) == 2) { // Lees 2 bytes (16 pinnen)
    // Lees de eerste byte (P0-P7)
    uint8_t buttonStates1 = Wire.read();
    // Lees de tweede byte (P8-P15)
    uint8_t buttonStates2 = Wire.read();
    Wire.endTransmission();

    // Combineer de twee bytes tot een 16-bit waarde
    uint16_t buttonStates = (buttonStates2 << 8) | buttonStates1;
    // Serial.print("buttonStates: ");
    // printBinary(buttonStates, 16);

    // Controleer welke button is ingedrukt met een while-loop
    int i = 0;                // Initialiseer de teller
    bool buttonFound = false; // Boolean om bij te houden of een button is gevonden
    while (i < 10 && !buttonFound) { // Ga door zolang i < 10 en geen button is gevonden
      int buttonState = (buttonStates & (1 << i));
      if (buttonState != 0) { // Als de pin hoog is (button ingedrukt)
        Serial.println("Button pressed: " + String(i));
        if (_buttonPressedCallback) {
          _buttonPressedCallback(i); // Roep de callback aan
        }
        buttonFound = true; // Zet de boolean op true om de loop te stoppen
      }
      i++; // Verhoog de teller
    }
  } else {
    Serial.println("I2C fout: Kan PCF8575 niet uitlezen!");
    Wire.endTransmission(); // Zorg ervoor dat de I2C-transmissie wordt beëindigd
  }
}

void PrioInputPanel::printBinary(int v, int num_places) {
  int mask = 0, n;

  for (n = 1; n <= num_places; n++) {
    mask = (mask << 1) | 0x0001;
  }
  v = v & mask; // truncate v to specified number of places

  while (num_places) {
    if (v & (0x0001 << (num_places - 1))) {
      Serial.print("1");
    } else {
      Serial.print("0");
    }

    --num_places;
    if (((num_places % 4) == 0) && (num_places != 0)) {
      Serial.print("_");
    }
  }
  Serial.println("");
}

void IRAM_ATTR PrioInputPanel::keyPressedISR() {
  if (_instance) {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - _instance->_lastDebounceTime;

    // Controleer of de debounce-tijd is verstreken (ook na overloop van millis())
    if (elapsedTime >= _instance->_debounceDelay) {
      _instance->_keyPressed = true;
      _instance->_lastDebounceTime = currentTime;
    }
  }
}