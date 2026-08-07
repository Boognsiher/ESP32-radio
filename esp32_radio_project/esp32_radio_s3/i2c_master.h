#pragma once
#include <Arduino.h>

// I2C-Master-Seite des in i2c_protocol.h definierten Protokolls.
// Alle Mehrbyte-Antworten werden per Checksumme geprüft; unplausible
// oder fehlende Antworten führen zu false (Aufrufer behält alten Stand).
namespace I2cMaster {
  void begin();

  // Fragt den Taster-Status ab. Gibt true zurück, wenn seit der letzten
  // Abfrage ein neuer Tastendruck registriert wurde (newStation enthält
  // dann den gewünschten Sender-Index).
  bool pollButtons(uint8_t &newStation);

  void triggerScan();

  struct ScanStatus { uint8_t state; uint8_t count; };
  bool getScanStatus(ScanStatus &out);

  struct ScanDevice { String name; String addr; int8_t rssi; };
  bool getScanDevice(uint8_t index, ScanDevice &out);

  void setBtTarget(const String &name);

  // Verbindet künftig direkt per fester MAC-Adresse (robuster als die
  // Namenssuche, siehe README). mac muss auf 6 Byte zeigen.
  void setBtTargetMac(const uint8_t mac[6]);

  // Entfernt eine gesetzte feste MAC-Adresse wieder -- DevKit verbindet
  // danach wieder per Namenssuche (setBtTarget()).
  void clearBtTargetMac();
}
