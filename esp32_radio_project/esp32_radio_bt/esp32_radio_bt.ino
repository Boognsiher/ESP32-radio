/*
 * ESP32 DevKitV1 - Bluetooth Audio Bridge + Taster (I2C-Slave)
 * ============================================================================
 * Empfaengt Audio per I2S vom Xiao S3, sendet per Bluetooth A2DP an einen
 * externen Lautsprecher. Liest 3 Taster ein und meldet sie per I2C ans S3.
 * Details siehe CLAUDE.md (verbindliche Spezifikation).
 *
 * Zweiter Anlauf (siehe CLAUDE.md "Zweiter Anlauf"): Audio-Pipeline baut
 * auf arduino-audio-tools (I2SStream -> A2DPStream) statt auf
 * Eigenbau-I2S-/BluetoothA2DPSource-Code auf.
 *
 * BT-Gerätename ändern: im Serial Monitor "setbt:GERAETENAME" eingeben.
 * BT-Geräte in der Naehe auflisten: "scan" eingeben.
 *
 * Benötigte Libraries (Library Manager):
 *   - "arduino-audio-tools" (pschatzmann)
 *   - "ESP32-A2DP" (pschatzmann)
 */

#include "config.h"
#include "storage.h"
#include "bt_audio.h"
#include "buttons.h"
#include "bt_scan.h"
#include "i2c_slave.h"

void setup() {
  Serial.begin(115200);
  Serial.println("\n[ESP32 BT Bridge] Start");
  Serial.println("BT-Name aendern: setbt:GERAETENAME eingeben");

  buttonsInit();
  i2cSlaveInit();

  String btName = storageLoadBtName();
  Serial.printf("[BT] Geraetename: %s\n", btName.c_str());
  btAudioInit(btName);   // initialisiert auch den BT-Controller/Bluedroid-Stack

  btScanInit();          // GAP-Callback erst NACH btAudioInit() registrieren,
                         // da dieser den BT-Stack hochfaehrt
}

void loop() {
  buttonsLoop();
  btAudioLoop();

  // Vom Webinterface (per I2C) ausgeloester BT-Zielwechsel
  String newName;
  if (i2cSlaveConsumePendingBtName(newName)) {
    storageSaveBtName(newName);
    Serial.printf("[BT] Neues Ziel per Web gesetzt: %s\n", newName.c_str());
    Serial.println("[BT] Neustart in 1 Sekunde...");
    delay(1000);
    ESP.restart();
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.startsWith("setbt:")) {
      String name = cmd.substring(6);
      name.trim();
      if (name.length() > 0) {
        storageSaveBtName(name);
        Serial.printf("[BT] Neuer Name gespeichert: %s\n", name.c_str());
        Serial.println("[BT] Neustart in 2 Sekunden...");
        delay(2000);
        ESP.restart();
      }
    } else if (cmd == "status") {
      Serial.printf("[BT] Verbunden: %s | Sender: %d\n",
        btAudioIsConnected() ? "JA" : "NEIN", buttonCurrentStation + 1);
    } else if (cmd == "scan") {
      btScanStart();
    }
  }

  delay(10);
}
