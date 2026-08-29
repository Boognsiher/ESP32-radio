#include "buttons.h"
#include "config.h"

volatile uint8_t buttonCurrentStation = 0;
volatile uint8_t buttonEventCounter   = 0;

static unsigned long lastBtn1 = 0, lastBtn2 = 0, lastBtn3 = 0;

static void selectStation(uint8_t idx) {
  buttonCurrentStation = idx;
  buttonEventCounter++;
}

void buttonsInit() {
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);
}

void buttonsLoop() {
  unsigned long now = millis();
  if (digitalRead(BTN_1) == LOW && now - lastBtn1 > DEBOUNCE_MS) { lastBtn1 = now; selectStation(0); }
  if (digitalRead(BTN_2) == LOW && now - lastBtn2 > DEBOUNCE_MS) { lastBtn2 = now; selectStation(1); }
  if (digitalRead(BTN_3) == LOW && now - lastBtn3 > DEBOUNCE_MS) { lastBtn3 = now; selectStation(2); }
}
