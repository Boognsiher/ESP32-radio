#pragma once
#include <Arduino.h>

// Bluetooth-A2DP-Quelle (pschatzmann/ESP32-A2DP): sendet die per I2S
// empfangenen PCM-Daten an den konfigurierten Lautsprecher.
//
// Zwei Verbindungsarten, in NVS persistiert:
//  - feste MAC-Adresse (bevorzugt, falls gesetzt): nutzt
//    set_auto_reconnect(esp_bd_addr_t, retries) + start() ohne Namen --
//    verbindet laut Library-Quellcode direkt ohne Discovery-Scan, sobald
//    eine Adresse hinterlegt ist. Deutlich zuverlässiger als Namenssuche.
//  - Gerätename (Fallback, falls keine MAC gesetzt ist): klassische
//    Discovery-basierte Namenssuche wie bisher.
namespace BtA2dp {
  void begin();
  bool isConnected();
  String deviceName();

  // Temporäre Diagnose für die Fehlersuche "verbunden + ON AIR, aber
  // kein Ton" -- zeigt periodisch, ob dataCallback() läuft und ob echte
  // (nicht-stille) Samples über I2S ankommen. In loop() aufrufen.
  void printAudioDebug();

  // Trennt eine bestehende Verbindung aktiv, schaltet Auto-Reconnect ab
  // (Library-Verhalten). ACHTUNG: NICHT unmittelbar vor
  // esp_bt_gap_start_discovery() aufrufen -- im Hardware-Test brachte
  // diese Reihenfolge (auch mit Warten auf die Trennungsbestätigung) den
  // kompletten Classic-BT-Stack zum Hängen (siehe bt_scan.cpp). Deshalb
  // ruft BtScan::start() das bewusst NICHT auf; diese Funktion steht nur
  // für andere, unabhängige Anwendungsfälle bereit.
  void disconnect();

  // Menschenlesbares Label des aktuell konfigurierten Ziels ("MAC AA:.."
  // oder der Gerätename), für Status-Ausgaben (Serial/Web).
  String targetLabel();

  // Speichert neuen Zielnamen in NVS und startet das Board neu, damit
  // sich die A2DP-Quelle mit dem neuen Ziel verbindet. Setzt keine MAC
  // zurück -- falls eine MAC hinterlegt ist, hat sie weiterhin Vorrang.
  void setDeviceName(const String &name);

  // Speichert eine feste Ziel-MAC-Adresse in NVS und startet das Board
  // neu. mac muss auf 6 Byte zeigen. Hat Vorrang vor dem Gerätenamen.
  void setDeviceMac(const uint8_t mac[6]);

  // Entfernt eine gesetzte feste MAC-Adresse wieder und startet neu --
  // danach verbindet sich das Board wieder per Namenssuche.
  void clearDeviceMac();
}
