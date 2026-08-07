#include "i2c_slave.h"
#include "config.h"
#include "i2c_protocol.h"
#include "buttons.h"
#include "bt_scan.h"
#include "bt_a2dp.h"
#include <Arduino.h>
#include <Wire.h>
#include <string.h>

namespace {
  volatile uint8_t lastCommand = I2C_CMD_GET_BUTTONS;
  volatile uint8_t requestedDeviceIdx = 0;

  volatile bool pendingBtTarget = false;
  char pendingBtName[I2C_BT_NAME_MAXLEN + 1] = "";

  // MAC-Kommando: pendingBtMacSet -> neue MAC übernehmen, pendingBtMacClear
  // -> feste MAC entfernen (Payload war leer, siehe i2c_protocol.h).
  volatile bool pendingBtMacSet = false;
  volatile bool pendingBtMacClear = false;
  uint8_t pendingBtMacBytes[6];

  // Läuft im I2C-ISR-Kontext -- kurz halten, keine Neustarts/Delays hier.
  void onReceive(int len) {
    if (Wire.available() < 1) return;
    uint8_t cmd = Wire.read();

    if (cmd == I2C_CMD_SET_BT_TARGET) {
      int i = 0;
      while (Wire.available() && i < I2C_BT_NAME_MAXLEN) pendingBtName[i++] = (char)Wire.read();
      pendingBtName[i] = 0;
      while (Wire.available()) Wire.read();  // Rest verwerfen, falls zu lang
      pendingBtTarget = true;
      lastCommand = I2C_CMD_GET_BUTTONS;
      return;
    }

    if (cmd == I2C_CMD_SET_BT_MAC) {
      int i = 0;
      while (Wire.available() && i < 6) pendingBtMacBytes[i++] = (uint8_t)Wire.read();
      while (Wire.available()) Wire.read();  // Rest verwerfen, falls zu lang
      if (i == 6) pendingBtMacSet = true;
      else if (i == 0) pendingBtMacClear = true;
      // 1..5 Byte: unvollständige/fehlerhafte Übertragung -> ignorieren
      lastCommand = I2C_CMD_GET_BUTTONS;
      return;
    }

    while (Wire.available()) Wire.read();  // evtl. Restbytes verwerfen

    if (cmd == I2C_CMD_START_SCAN) {
      BtScan::start();
      lastCommand = I2C_CMD_GET_BUTTONS;
    } else if (cmd == I2C_CMD_GET_SCAN_STATUS) {
      lastCommand = I2C_CMD_GET_SCAN_STATUS;
    } else if (cmd >= I2C_CMD_GET_SCAN_DEVICE && cmd < I2C_CMD_GET_SCAN_DEVICE + I2C_SCAN_MAX_DEVICES) {
      lastCommand = cmd;
      requestedDeviceIdx = cmd - I2C_CMD_GET_SCAN_DEVICE;
    } else {
      lastCommand = I2C_CMD_GET_BUTTONS;
    }
  }

  void onRequest() {
    if (lastCommand == I2C_CMD_GET_SCAN_STATUS) {
      uint8_t buf[3] = { BtScan::state(), BtScan::count(), 0 };
      buf[2] = i2cChecksum(buf, 2);
      Wire.write(buf, 3);
    } else if (lastCommand >= I2C_CMD_GET_SCAN_DEVICE) {
      uint8_t buf[I2C_SCAN_RECORD_LEN];
      memset(buf, 0, sizeof(buf));
      char name[I2C_SCAN_NAME_LEN];
      uint8_t addr[6];
      int8_t rssi = 0;
      if (BtScan::getDevice(requestedDeviceIdx, name, addr, rssi)) {
        memcpy(buf, name, I2C_SCAN_NAME_LEN);
        memcpy(buf + I2C_SCAN_NAME_LEN, addr, 6);
        buf[I2C_SCAN_NAME_LEN + 6] = (uint8_t)rssi;
      }
      buf[I2C_SCAN_RECORD_LEN - 1] = i2cChecksum(buf, I2C_SCAN_RECORD_LEN - 1);
      Wire.write(buf, I2C_SCAN_RECORD_LEN);
    } else {
      uint8_t buf[3] = { Buttons::eventCounter(), Buttons::currentStation(), 0 };
      buf[2] = i2cChecksum(buf, 2);
      Wire.write(buf, 3);
    }
    lastCommand = I2C_CMD_GET_BUTTONS;  // zurücksetzen für die nächste einfache Abfrage
  }
}

namespace I2cSlave {

void begin() {
  Wire.begin((uint8_t)I2C_SLAVE_ADDR, PIN_I2C_SDA, PIN_I2C_SCL, I2C_CLOCK_HZ);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
  Serial.println("[I2C] Slave bereit (0x42)");
}

void handlePendingBtTarget() {
  if (pendingBtMacSet) {
    pendingBtMacSet = false;
    Serial.println("[BT] Neue Ziel-MAC per I2C/Web gesetzt");
    BtA2dp::setDeviceMac(pendingBtMacBytes);  // führt intern ESP.restart() aus
    return;
  }
  if (pendingBtMacClear) {
    pendingBtMacClear = false;
    Serial.println("[BT] Ziel-MAC per I2C/Web entfernt");
    BtA2dp::clearDeviceMac();  // führt intern ESP.restart() aus
    return;
  }
  if (!pendingBtTarget) return;
  pendingBtTarget = false;
  String newName(pendingBtName);
  if (newName.length() > 0) {
    Serial.printf("[BT] Neues Ziel per I2C/Web gesetzt: %s\n", newName.c_str());
    BtA2dp::setDeviceName(newName);  // führt intern ESP.restart() aus
  }
}

} // namespace I2cSlave
