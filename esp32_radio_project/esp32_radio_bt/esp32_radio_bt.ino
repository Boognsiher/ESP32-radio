/*
 * ESP32 DevKitV1 -- Bluetooth-A2DP-Bridge + Taster (I2C-Slave)
 * ===========================================================================
 * Empfängt Audio per I2S vom Xiao S3, gibt es per Bluetooth A2DP an einen
 * konfigurierten Lautsprecher weiter (Auto-Reconnect), liest 3 Sender-
 * Taster, misst optional die Raumtemperatur (DS18B20) und beantwortet
 * I2C-Anfragen des S3 (Taster-Status, BT-Geräte-Scan, Raumtemperatur,
 * neues BT-Ziel per Name oder fester MAC-Adresse setzen).
 *
 * Verbindung per fester MAC-Adresse (falls gesetzt) hat Vorrang vor der
 * Namenssuche und ist zuverlässiger, da kein Discovery-Scan nötig ist.
 *
 * Der DS18B20-Temperatursensor ist optional: ohne angeschlossenen Sensor
 * meldet RoomSensor::isValid() dauerhaft false, der S3 zeigt dann "n/a".
 *
 * Vollständige Spezifikation: ../CLAUDE.md
 * Verkabelung: ../hardware/pinout.md
 *
 * Serial-Kommandos (115200 Baud):
 *   setbt:GERAETENAME          neues BT-Ziel per Name setzen (Neustart)
 *   setbtmac:AA:BB:CC:DD:EE:FF neues BT-Ziel per fester MAC setzen (Neustart)
 *   clearbtmac                  feste MAC entfernen, zurück auf Namenssuche
 *   scan                         ca. 12s nach sichtbaren BT-Classic-Geräten suchen
 *                                 (nur möglich, wenn gerade NICHT verbunden)
 *   stopbt                       aktuelle BT-Verbindung trennen, kein Auto-
 *                                 Reconnect (setbt/setbtmac erneut zum Fortsetzen)
 *   status                        aktueller Status ausgeben
 *
 * Benötigte Libraries (Board-Package **2.0.x** -- siehe CLAUDE.md
 * Stolperstein #2):
 *   - "ESP32-A2DP" von pschatzmann
 *   - "OneWire" von Paul Stoffregen
 *   - "DallasTemperature" von milesburton
 */

#include "config.h"
#include "i2s_audio.h"
#include "bt_a2dp.h"
#include "buttons.h"
#include "bt_scan.h"
#include "i2c_slave.h"
#include "serial_console.h"
#include "room_sensor.h"

void setup() {
  Serial.begin(115200);
  Serial.println("\n[ESP32 BT-Bridge] Start");
  Serial.println("BT-Ziel: setbt:NAME | setbtmac:AA:BB:CC:DD:EE:FF | clearbtmac | scan | stopbt | status");

  Buttons::begin();
  I2sAudio::begin();
  BtScan::begin();
  I2cSlave::begin();
  RoomSensor::begin();
  BtA2dp::begin();
}

void loop() {
  Buttons::poll();
  RoomSensor::loop();
  I2cSlave::handlePendingBtTarget();
  SerialConsole::poll();
  BtScan::loop();

  static unsigned long lastAudioDbg = 0;
  if (millis() - lastAudioDbg > 3000) {
    lastAudioDbg = millis();
    BtA2dp::printAudioDebug();
  }

  delay(10);
}
