#pragma once
#include <Arduino.h>

// I2C-Master-Seite des in i2c_protocol.h definierten Protokolls.
// Alle Mehrbyte-Antworten werden per Checksumme geprüft; unplausible
// oder fehlende Antworten führen zu false (Aufrufer behält alten Stand).
namespace I2cMaster {
  void begin();

  // Fragt den Taster-Status ab (liefert bei jeder gültigen Antwort auch
  // btConnected mit -- siehe I2C_CMD_GET_BUTTONS in i2c_protocol.h für
  // die Begründung, warum der BT-Status huckepack hier mitkommt statt
  // über ein eigenes Kommando). Rückgabewert: true = gültige Antwort
  // gelesen (Checksumme ok, Werte plausibel) -- btConnectedOut ist dann
  // aktuell. stationChanged zusätzlich true, wenn sich seit der letzten
  // Abfrage ein neuer Tastendruck ergeben hat (newStation enthält dann
  // den gewünschten Sender-Index, oder den Sonderwert
  // I2C_BTN_EVENT_SHOW_IP bei der Taster-Kombi 1+2 -- siehe radio.cpp).
  bool pollButtons(uint8_t &newStation, bool &stationChanged, bool &btConnectedOut);

  void triggerScan();

  struct ScanStatus { uint8_t state; uint8_t count; };
  bool getScanStatus(ScanStatus &out);

  // Fragt die Raumtemperatur vom DevKit ab (DS18B20, optional). false =
  // keine/unplausible Antwort -- Aufrufer behält alten Stand. outValid
  // spiegelt den valid-Flag aus dem Protokoll (false = kein Sensor
  // angeschlossen bzw. Lesefehler auf DevKit-Seite).
  bool getRoomTemp(float &outCelsius, bool &outValid);

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
