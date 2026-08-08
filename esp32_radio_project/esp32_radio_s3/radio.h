#pragma once
#include <Arduino.h>

// Zentraler Koordinator: verbindet Stations/Display/AudioStream/I2cMaster
// zu einem konsistenten Radio-Zustand. Webinterface und Taster-Polling
// rufen beide nur Radio::startStation() auf, damit es nur eine
// Quelle der Wahrheit für "aktueller Sender/Status" gibt.
namespace Radio {
  void begin();
  void startStation(int idx);

  // Stoppt die Wiedergabe bewusst (z.B. "Stumm bis ich zurueck bin" über
  // Webinterface) -- anders als ein Verbindungsfehler löst das KEINEN
  // Auto-Reconnect aus. startStation() erneut aufrufen, um fortzusetzen.
  void stop();

  void loop();

  int currentStation();
  bool isPlaying();
  String statusMessage();
  String currentArtist();
  String currentTitle();

  void setWifiConnected(bool connected);
  bool wifiConnected();

  // Zuletzt per I2C abgefragte Raumtemperatur (DS18B20 am DevKit,
  // optional). roomTempValid()==false, solange kein Sensor angeschlossen
  // ist oder noch keine plausible Antwort einging.
  bool roomTempValid();
  float roomTempC();
}
