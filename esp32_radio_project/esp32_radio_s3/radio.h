#pragma once
#include <Arduino.h>

// Zentraler Koordinator: verbindet Stations/Display/AudioStream/I2cMaster
// zu einem konsistenten Radio-Zustand. Webinterface und Taster-Polling
// rufen beide nur Radio::startStation() auf, damit es nur eine
// Quelle der Wahrheit für "aktueller Sender/Status" gibt.
namespace Radio {
  void begin();
  void startStation(int idx);
  void loop();

  int currentStation();
  bool isPlaying();
  String statusMessage();
  String currentArtist();
  String currentTitle();

  void setWifiConnected(bool connected);
  bool wifiConnected();
}
