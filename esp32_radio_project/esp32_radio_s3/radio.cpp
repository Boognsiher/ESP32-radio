#include "radio.h"
#include "config.h"
#include "stations.h"
#include "display.h"
#include "audio_stream.h"
#include "i2c_master.h"
#include <WiFi.h>

namespace {
  int    station = 0;
  bool   playing = false;
  String status  = "Starte...";
  String artist, title;
  bool   wifiOk  = false;
  unsigned long lastI2cPoll = 0;

  void onTrackInfo(const String &a, const String &t) {
    artist = a; title = t;
    Display::setTrackInfo(artist, title);
  }

  void onStreamEnd() {
    status = "Reconnect...";
    playing = false;
    Display::setStatus(status, playing);
    if (wifiOk) {
      AudioStream::play(Stations::get(station).url);
      status = "Spielt";
      playing = true;
      Display::setStatus(status, playing);
    }
  }
}

namespace Radio {

void begin() {
  Stations::begin();
  station = Stations::loadCurrentIndex();
  AudioStream::setCallbacks(onTrackInfo, onStreamEnd);
}

void startStation(int idx) {
  if (idx < 0 || idx >= STATION_COUNT) return;
  station = idx;
  artist = ""; title = "";
  status = "Verbinde...";
  playing = false;

  Display::showStation(station, STATION_COUNT, Stations::get(station).name);
  Display::setTrackInfo(artist, title);
  Display::setStatus(status, playing);

  if (wifiOk) {
    AudioStream::play(Stations::get(station).url);
    status = "Spielt";
    playing = true;
    Display::setStatus(status, playing);
  }

  Stations::saveCurrentIndex(station);
}

void loop() {
  AudioStream::loop();
  Display::tick();

  unsigned long now = millis();
  if (now - lastI2cPoll > I2C_POLL_INTERVAL_MS) {
    lastI2cPoll = now;
    uint8_t newStation;
    if (I2cMaster::pollButtons(newStation)) startStation(newStation);
  }

  bool nowConnected = (WiFi.status() == WL_CONNECTED);
  if (wifiOk && !nowConnected) {
    setWifiConnected(false);
    status = "WLAN VERLOREN";
    playing = false;
    Display::setStatus(status, playing);
  } else if (!wifiOk && nowConnected) {
    setWifiConnected(true);
    startStation(station);
  }
}

int currentStation() { return station; }
bool isPlaying() { return playing; }
String statusMessage() { return status; }
String currentArtist() { return artist; }
String currentTitle() { return title; }

void setWifiConnected(bool connected) {
  wifiOk = connected;
  Display::setWifiInfo(wifiOk, wifiOk ? WiFi.localIP().toString() : "");
}

bool wifiConnected() { return wifiOk; }

} // namespace Radio
