#pragma once
#include <Arduino.h>

// Ansteuerung des runden 1.5" ST77916-QSPI-Displays im Phosphor-Grün-
// Retro-Stil. Redraw-Strategie ist bewusst ereignisgesteuert (CLAUDE.md
// Stolperstein #5): showStation()/setTrackInfo()/setInfoLine() lösen
// nur bei tatsächlicher Änderung einen (Teil-)Redraw aus, tick()
// aktualisiert Scroll-Text und ON-AIR-Blinken jeweils nur zeilenweise.
//
// Kein eigener Wetter-Bildschirm: statt dessen läuft eine Info-Zeile
// permanent im Radio-Screen mit (ersetzt die frühere IP-Anzeige dort --
// die IP bleibt über mDNS "esp32radio.local" bzw. Webinterface
// erreichbar). radio.cpp rotiert ihren Inhalt alle 30s zwischen Raum-/
// Aussentemperatur, heutigem und morgigem Wetter (setInfoLine()) --
// Display kennt nur den fertigen Text, keine Wetter-Logik.
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

  // Speichert WLAN-Status intern (aktuell nicht mehr auf dem Display
  // dargestellt, siehe Kommentar oben); kein Redraw nötig.
  void setWifiInfo(bool connected, const String &ip);

  // Aktualisiert die untere Info-Zeile (nur diese Zeile, kein Vollbild-
  // Clear) -- nur bei tatsächlicher Textänderung wird neu gezeichnet.
  void setInfoLine(const String &text);

  // Muss regelmässig aus loop() aufgerufen werden: kümmert sich intern
  // um Scroll-Text (alle 350ms) und ON-AIR-Blinken (alle 800ms).
  void tick();
}
