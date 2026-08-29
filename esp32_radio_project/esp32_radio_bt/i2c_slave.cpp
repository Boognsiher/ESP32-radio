#include "i2c_slave.h"
#include "config.h"
#include "buttons.h"
#include "bt_scan.h"
#include <Wire.h>

static volatile uint8_t lastCommand      = I2C_CMD_BUTTONS;
static volatile uint8_t requestedDevIdx  = 0;

static volatile bool pendingBtNameChange = false;
static char pendingBtName[BT_NAME_MAX_LEN + 1] = "";

// Protokoll (siehe esp32_radio_s3/i2c_link.h fuer die Master-Seite):
// Der Master schreibt optional zuerst 1 Kommando-Byte, danach wird per
// requestFrom() die passende Antwort abgeholt. Ohne vorheriges Schreiben
// gilt automatisch I2C_CMD_BUTTONS (Standardfall bei jeder Taster-Abfrage).
static void onReceive(int len) {
  if (Wire.available() < 1) return;
  uint8_t cmd = Wire.read();

  if (cmd == I2C_CMD_SET_BT_NAME) {
    int i = 0;
    while (Wire.available() && i < BT_NAME_MAX_LEN) pendingBtName[i++] = (char)Wire.read();
    pendingBtName[i] = 0;
    while (Wire.available()) Wire.read();   // Rest verwerfen falls zu lang
    pendingBtNameChange = true;
    lastCommand = I2C_CMD_BUTTONS;
    return;
  }

  while (Wire.available()) Wire.read();   // evtl. Restbytes verwerfen

  if (cmd == I2C_CMD_START_SCAN) {
    btScanStart();
    lastCommand = I2C_CMD_BUTTONS;
  } else if (cmd == I2C_CMD_SCAN_STATUS) {
    lastCommand = I2C_CMD_SCAN_STATUS;
  } else if (cmd >= I2C_CMD_SCAN_DEVICE && cmd < I2C_CMD_SCAN_DEVICE + SCAN_MAX_DEVICES) {
    lastCommand = cmd;
    requestedDevIdx = cmd - I2C_CMD_SCAN_DEVICE;
  } else {
    lastCommand = I2C_CMD_BUTTONS;
  }
}

static void onRequest() {
  if (lastCommand == I2C_CMD_SCAN_STATUS) {
    uint8_t buf[2] = { scanState, scanDeviceCount };
    Wire.write(buf, 2);
  } else if (lastCommand >= I2C_CMD_SCAN_DEVICE) {
    uint8_t buf[SCAN_NAME_LEN + 6 + 1];
    memset(buf, 0, sizeof(buf));
    uint8_t idx = requestedDevIdx;
    if (idx < scanDeviceCount) {
      memcpy(buf, scanDevices[idx].name, SCAN_NAME_LEN);
      memcpy(buf + SCAN_NAME_LEN, scanDevices[idx].addr, 6);
      buf[SCAN_NAME_LEN + 6] = (uint8_t)scanDevices[idx].rssi;
    }
    Wire.write(buf, sizeof(buf));
  } else {
    uint8_t buf[2] = { buttonEventCounter, buttonCurrentStation };
    Wire.write(buf, 2);
  }
  lastCommand = I2C_CMD_BUTTONS;   // zurueckgesetzt fuer naechste einfache Abfrage
}

void i2cSlaveInit() {
  Wire.begin((uint8_t)I2C_SLAVE_ADDR, I2C_SDA, I2C_SCL, I2C_CLOCK_HZ);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
}

bool i2cSlaveConsumePendingBtName(String &outName) {
  if (!pendingBtNameChange) return false;
  pendingBtNameChange = false;
  outName = String(pendingBtName);
  return outName.length() > 0;
}
