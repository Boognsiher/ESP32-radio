#pragma once
#include <Arduino.h>
#include "i2c_protocol.h"

// Klassischer Bluetooth-Geräte-Scan (ESP-IDF GAP-Inquiry), Ergebnisse
// werden für die I2C-Abfrage durch den S3 zwischengespeichert.
namespace BtScan {
  void begin();     // registriert den GAP-Callback
  void start();      // löst einen Inquiry-Scan aus (nicht blockierend)

  uint8_t state();   // I2C_SCAN_STATE_*
  uint8_t count();

  // Rohdaten für Record #index. false, wenn index >= count().
  bool getDevice(uint8_t index, char nameOut[I2C_SCAN_NAME_LEN], uint8_t addrOut[6], int8_t &rssiOut);
}
