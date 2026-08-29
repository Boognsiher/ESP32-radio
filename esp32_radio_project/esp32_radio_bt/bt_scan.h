// BT-Geraete-Scan (Diagnose/Auswahl): listet sichtbare BT-Classic-Geraete
// per ESP-IDF GAP-Inquiry. Ergebnisse werden ueber I2C ans S3 gereicht.
#pragma once
#include <Arduino.h>
#include "config.h"

struct ScanDevice {
  char    name[SCAN_NAME_LEN + 1];
  uint8_t addr[6];
  int8_t  rssi;
};

void btScanInit();      // registriert den GAP-Callback
void btScanStart();     // startet Inquiry, nicht blockierend

extern volatile uint8_t scanDeviceCount;
extern volatile uint8_t scanState;   // 0=idle, 1=laeuft, 2=fertig
extern ScanDevice scanDevices[SCAN_MAX_DEVICES];
