#pragma once
#include <Arduino.h>

// Liest die 3 Sender-Taster mit Entprellung. eventCounter()/currentStation()
// werden auch aus dem I2C-Slave-onRequest-Callback (ISR-Kontext) gelesen,
// daher intern volatile.
namespace Buttons {
  void begin();
  void poll();                 // in loop() aufrufen

  uint8_t eventCounter();      // erhöht sich bei jedem gültigen Tastendruck
  uint8_t currentStation();    // zuletzt gewählter Sender-Index (0..2)
}
