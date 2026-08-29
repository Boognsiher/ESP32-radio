// I2C-Master: fragt Taster-Status vom DevKitV1 ab, steuert BT-Scan.
// Protokoll siehe CLAUDE.md ("I2C: Xiao S3 <-> ESP32 DevKitV1") und
// Stolperstein #6 (Plausibilitätsprüfungen gegen korrupte Antworten).
#pragma once
#include <Arduino.h>
#include "config.h"

struct ScanDeviceInfo {
  String  name;
  String  addr;
  int8_t  rssi;
};

void i2cLinkInit();

// Liefert per Callback den neu gewählten Sender-Index (0..STATION_COUNT-1),
// wenn sich der Taster-Zustand am DevKit geändert hat. Muss regelmässig
// (alle I2C_POLL_MS) aufgerufen werden.
void i2cLinkPollButtons(void (*onStationChanged)(int idx));

void i2cLinkTriggerBtScan();
void i2cLinkSetBtTarget(const String &name);

// Aktualisiert den internen Scan-Ergebnis-Cache; danach über die
// i2cLinkScan*-Getter auslesen.
void i2cLinkRefreshScanResults();

uint8_t i2cLinkScanState();   // 0=idle, 1=laeuft, 2=fertig
uint8_t i2cLinkScanCount();
const ScanDeviceInfo &i2cLinkScanDevice(uint8_t idx);
