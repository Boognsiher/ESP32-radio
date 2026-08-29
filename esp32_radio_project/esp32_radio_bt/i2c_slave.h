// I2C-Slave: beantwortet Taster-/Scan-Abfragen vom S3, nimmt neuen
// BT-Zielnamen entgegen. Protokoll siehe esp32_radio_s3/i2c_link.h.
#pragma once
#include <Arduino.h>

void i2cSlaveInit();

// Vom .ino in loop() pruefen: true wenn ein neuer BT-Name per Web/I2C
// gesetzt wurde (Aufrufer speichert ihn und startet neu).
bool i2cSlaveConsumePendingBtName(String &outName);
