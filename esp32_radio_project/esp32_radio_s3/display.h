#pragma once
#include <Arduino.h>

// Ansteuerung des runden 1.5" ST77916-QSPI-Displays im Phosphor-Grün-
// Retro-Stil. Redraw-Strategie ist bewusst ereignisgesteuert (CLAUDE.md
// Stolperstein #5): showStation()/setTrackInfo()/setWifiInfo() lösen
// nur bei tatsächlicher Änderung einen Vollbild-Redraw aus, tick()
// aktualisiert Scroll-Text und ON-AIR-Blinken jeweils nur zeilenweise.
namespace Display {
  void begin();

  // Vollbild-Meldung für Boot/Fehler/WLAN-Setup (kein Radio-Layout).
  void showMessage(const String &line1, const String &line2 = "");

  // Radio-Hauptbildschirm: kompletter Redraw. Nur bei Senderwechsel
  // aufrufen (setzt auch playing/status zurück auf den Aufrufer-Zustand
  // via setStatus()/setTrackInfo() danach).
  void showStation(int stationIndex, int stationCount, const String &stationName);

  // Aktualisiert die Status-/Blink-Zeile (nur diese Zeile, kein Vollbild-Clear).
  void setStatus(const String &statusText, bool isPlaying);

  // Löst bei tatsächlicher Änderung einen Vollbild-Redraw aus (CLAUDE.md:
  // Titeländerung gehört zu den Vollbild-Trigger-Ereignissen).
  void setTrackInfo(const String &artist, const String &title);

  // Löst bei Statuswechsel (verbunden <-> getrennt) einen Vollbild-Redraw
  // aus; reine IP-Aktualisierungen werden nur zeilenweise nachgezogen.
  void setWifiInfo(bool connected, const String &ip);

  // Muss regelmässig aus loop() aufgerufen werden: kümmert sich intern
  // um Scroll-Text (alle 350ms) und ON-AIR-Blinken (alle 800ms).
  void tick();
}
