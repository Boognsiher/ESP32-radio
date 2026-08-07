/*
 * ESP32 DevKitV1 -- Bluetooth-A2DP-Bridge + Taster (I2C-Slave)
 * ===========================================================================
 * Empfängt Audio per I2S vom Xiao S3, gibt es per Bluetooth A2DP an einen
 * konfigurierten Lautsprecher weiter (Auto-Reconnect), liest 3 Sender-
 * Taster und beantwortet I2C-Anfragen des S3 (Taster-Status, BT-Geräte-
 * Scan, neuen BT-Zielnamen setzen).
 *
 * Vollständige Spezifikation: ../CLAUDE.md
 * Verkabelung: ../hardware/pinout.md
 *
 * Serial-Kommandos (115200 Baud):
 *   setbt:GERAETENAME   neues BT-Ziel setzen (NVS-persistiert, Neustart)
 *   scan                 ca. 12s nach sichtbaren BT-Classic-Geräten suchen
 *   status                aktueller Status ausgeben
 *
 * Benötigte Library (Board-Package **2.0.x** -- siehe CLAUDE.md
 * Stolperstein #2):
 *   - "ESP32-A2DP" von pschatzmann
 */

#include "config.h"
#include "i2s_audio.h"
#include "bt_a2dp.h"
#include "buttons.h"
#include "bt_scan.h"
#include "i2c_slave.h"
#include "serial_console.h"

void setup() {
  Serial.begin(115200);
  Serial.println("\n[ESP32 BT-Bridge] Start");
  Serial.println("BT-Ziel aendern: setbt:GERAETENAME  |  Geraete suchen: scan  |  Status: status");

  Buttons::begin();
  I2sAudio::begin();
  BtScan::begin();
  I2cSlave::begin();
  BtA2dp::begin();
}

void loop() {
  Buttons::poll();
  I2cSlave::handlePendingBtTarget();
  SerialConsole::poll();
  delay(10);
}
