#include "buttons.h"
#include "config.h"

namespace {
  const uint8_t PINS[STATION_COUNT] = { PIN_BTN_1, PIN_BTN_2, PIN_BTN_3 };
  unsigned long lastPress[STATION_COUNT] = { 0, 0, 0 };
  volatile uint8_t evtCounter = 0;
  volatile uint8_t station = 0;
}

namespace Buttons {

void begin() {
  for (uint8_t i = 0; i < STATION_COUNT; i++) pinMode(PINS[i], INPUT_PULLUP);
}

void poll() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < STATION_COUNT; i++) {
    if (digitalRead(PINS[i]) == LOW && now - lastPress[i] > BUTTON_DEBOUNCE_MS) {
      lastPress[i] = now;
      station = i;
      evtCounter++;
      Serial.printf("[BTN] Sender %d gewaehlt (Event %d)\n", i + 1, evtCounter);
    }
  }
}

uint8_t eventCounter() { return evtCounter; }
uint8_t currentStation() { return station; }

} // namespace Buttons
