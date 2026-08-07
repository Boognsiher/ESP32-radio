#include "i2c_master.h"
#include "config.h"
#include "i2c_protocol.h"
#include <Wire.h>
#include <string.h>
#include <stdio.h>

namespace {
  bool synced = false;
  uint8_t lastEvent = 0;

  void sendCommand(uint8_t cmd) {
    Wire.beginTransmission(I2C_SLAVE_ADDR);
    Wire.write(cmd);
    Wire.endTransmission();
  }
}

namespace I2cMaster {

void begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_CLOCK_HZ);
}

bool pollButtons(uint8_t &newStation) {
  uint8_t got = Wire.requestFrom((int)I2C_SLAVE_ADDR, 3);
  if (got < 3 || Wire.available() < 3) return false;

  uint8_t buf[3];
  for (int i = 0; i < 3; i++) buf[i] = Wire.read();
  if (buf[2] != i2cChecksum(buf, 2)) return false;   // unplausibel -> verwerfen

  uint8_t evt = buf[0], idx = buf[1];
  if (idx >= STATION_COUNT) return false;

  if (!synced) { lastEvent = evt; synced = true; return false; }
  if (evt == lastEvent) return false;

  lastEvent = evt;
  newStation = idx;
  return true;
}

void triggerScan() {
  sendCommand(I2C_CMD_START_SCAN);
}

bool getScanStatus(ScanStatus &out) {
  sendCommand(I2C_CMD_GET_SCAN_STATUS);
  delay(5);
  uint8_t got = Wire.requestFrom((int)I2C_SLAVE_ADDR, 3);
  if (got < 3 || Wire.available() < 3) return false;

  uint8_t buf[3];
  for (int i = 0; i < 3; i++) buf[i] = Wire.read();
  if (buf[2] != i2cChecksum(buf, 2)) return false;
  if (buf[0] > I2C_SCAN_STATE_DONE || buf[1] > I2C_SCAN_MAX_DEVICES) return false;

  out.state = buf[0];
  out.count = buf[1];
  return true;
}

bool getScanDevice(uint8_t index, ScanDevice &out) {
  if (index >= I2C_SCAN_MAX_DEVICES) return false;
  sendCommand(I2C_CMD_GET_SCAN_DEVICE + index);
  delay(5);
  uint8_t got = Wire.requestFrom((int)I2C_SLAVE_ADDR, I2C_SCAN_RECORD_LEN);
  if (got < I2C_SCAN_RECORD_LEN || Wire.available() < I2C_SCAN_RECORD_LEN) return false;

  uint8_t buf[I2C_SCAN_RECORD_LEN];
  for (int i = 0; i < I2C_SCAN_RECORD_LEN; i++) buf[i] = Wire.read();
  if (buf[I2C_SCAN_RECORD_LEN - 1] != i2cChecksum(buf, I2C_SCAN_RECORD_LEN - 1)) return false;

  char nameBuf[I2C_SCAN_NAME_LEN + 1];
  memcpy(nameBuf, buf, I2C_SCAN_NAME_LEN);
  nameBuf[I2C_SCAN_NAME_LEN] = 0;

  const uint8_t *addr = buf + I2C_SCAN_NAME_LEN;
  bool addrAllZero = true;
  for (int i = 0; i < 6; i++) if (addr[i] != 0) { addrAllZero = false; break; }
  if (addrAllZero && nameBuf[0] == 0) return false;  // leerer/nicht befüllter Slot

  char addrStr[18];
  snprintf(addrStr, sizeof(addrStr), "%02X:%02X:%02X:%02X:%02X:%02X",
    addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

  out.name = String(nameBuf);
  out.addr = String(addrStr);
  out.rssi = (int8_t)buf[I2C_SCAN_NAME_LEN + 6];
  return true;
}

void setBtTarget(const String &name) {
  String trimmed = name;
  if (trimmed.length() > I2C_BT_NAME_MAXLEN) trimmed = trimmed.substring(0, I2C_BT_NAME_MAXLEN);
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(I2C_CMD_SET_BT_TARGET);
  Wire.write((const uint8_t *)trimmed.c_str(), trimmed.length());
  Wire.endTransmission();
}

void setBtTargetMac(const uint8_t mac[6]) {
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(I2C_CMD_SET_BT_MAC);
  Wire.write(mac, 6);
  Wire.endTransmission();
}

void clearBtTargetMac() {
  // Kommando ohne Payload -- der Slave interpretiert das als "MAC entfernen"
  // (siehe i2c_protocol.h).
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(I2C_CMD_SET_BT_MAC);
  Wire.endTransmission();
}

} // namespace I2cMaster
