#include "i2c_link.h"
#include <Wire.h>

static uint8_t lastButtonEvent = 0;
static bool    synced          = false;

static ScanDeviceInfo scanResults[SCAN_MAX_DEVICES];
static uint8_t scanCount = 0;
static uint8_t scanState = 0;

void i2cLinkInit() {
  Wire.begin(I2C_SDA, I2C_SCL, I2C_CLOCK_HZ);
}

static void sendCommand(uint8_t cmd) {
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(cmd);
  Wire.endTransmission();
}

void i2cLinkPollButtons(void (*onStationChanged)(int idx)) {
  uint8_t received = Wire.requestFrom(I2C_SLAVE_ADDR, 2);
  if (received < 2 || Wire.available() < 2) return;  // keine/kaputte Antwort, naechster Poll versucht es erneut

  uint8_t evt = Wire.read();
  uint8_t idx = Wire.read();

  if (!synced) {
    lastButtonEvent = evt;
    synced = true;
    return;
  }

  if (evt != lastButtonEvent) {
    lastButtonEvent = evt;
    if (idx < STATION_COUNT && onStationChanged) onStationChanged(idx);
  }
}

void i2cLinkTriggerBtScan() {
  sendCommand(I2C_CMD_START_SCAN);
  scanState = 1;
  scanCount = 0;
}

void i2cLinkSetBtTarget(const String &name) {
  String trimmed = name;
  if (trimmed.length() > BT_NAME_MAX_LEN) trimmed = trimmed.substring(0, BT_NAME_MAX_LEN);
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(I2C_CMD_SET_BT_NAME);
  Wire.write((const uint8_t *)trimmed.c_str(), trimmed.length());
  Wire.endTransmission();
}

// Stolperstein #6: einfache Plausibilitätsprüfungen, damit korrupte
// I2C-Antworten (z.B. durch fehlende Pull-ups/Wackelkontakt, siehe
// Stolperstein #13) nicht als Ergebnis angezeigt werden.
void i2cLinkRefreshScanResults() {
  sendCommand(I2C_CMD_SCAN_STATUS);
  delay(5);
  Wire.requestFrom(I2C_SLAVE_ADDR, 2);
  if (Wire.available() < 2) return;
  uint8_t newState = Wire.read();
  uint8_t count    = Wire.read();

  if (newState > 2 || count > SCAN_MAX_DEVICES) return;  // unplausibel, alte Werte behalten
  scanState = newState;
  scanCount = count;

  if (scanState != 2) return;  // nur bei "fertig" Geraete abholen

  uint8_t validCount = 0;
  for (uint8_t i = 0; i < scanCount; i++) {
    sendCommand(I2C_CMD_SCAN_DEVICE + i);
    delay(5);
    uint8_t need = SCAN_NAME_LEN + 6 + 1;
    Wire.requestFrom(I2C_SLAVE_ADDR, need);
    if (Wire.available() < need) continue;

    char nameBuf[SCAN_NAME_LEN + 1];
    for (int b = 0; b < SCAN_NAME_LEN; b++) nameBuf[b] = Wire.read();
    nameBuf[SCAN_NAME_LEN] = 0;

    uint8_t addr[6];
    for (int b = 0; b < 6; b++) addr[b] = Wire.read();
    int8_t rssi = (int8_t)Wire.read();

    bool addrAllZero = true;
    for (int b = 0; b < 6; b++) if (addr[b] != 0) { addrAllZero = false; break; }
    if (addrAllZero && nameBuf[0] == 0) continue;  // vermutlich leerer/korrupter Slot

    char addrStr[18];
    snprintf(addrStr, sizeof(addrStr), "%02X:%02X:%02X:%02X:%02X:%02X",
      addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    scanResults[validCount].name = String(nameBuf);
    scanResults[validCount].addr = String(addrStr);
    scanResults[validCount].rssi = rssi;
    validCount++;
  }
  scanCount = validCount;
}

uint8_t i2cLinkScanState() { return scanState; }
uint8_t i2cLinkScanCount() { return scanCount; }
const ScanDeviceInfo &i2cLinkScanDevice(uint8_t idx) { return scanResults[idx]; }
