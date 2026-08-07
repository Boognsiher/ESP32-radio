#pragma once
#include <Arduino.h>

// Bluetooth-A2DP-Quelle (pschatzmann/ESP32-A2DP): sendet die per I2S
// empfangenen PCM-Daten an den konfigurierten Lautsprecher, mit
// Auto-Reconnect. Gerätename ist in NVS persistiert.
namespace BtA2dp {
  void begin();
  bool isConnected();
  String deviceName();

  // Speichert neuen Zielnamen in NVS und startet das Board neu, damit
  // sich die A2DP-Quelle mit dem neuen Ziel verbindet.
  void setDeviceName(const String &name);
}
