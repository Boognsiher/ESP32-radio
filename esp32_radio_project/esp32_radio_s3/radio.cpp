#include "radio.h"
#include "config.h"
#include "stations.h"
#include "display.h"
#include "audio_stream.h"
#include "i2c_master.h"
#include "weather.h"
#include <WiFi.h>
#include <math.h>

namespace {
  int    station = 0;
  bool   playing = false;
  String status  = "Starte...";
  String artist, title;
  bool   wifiOk  = false;
  unsigned long lastI2cPoll = 0;

  // --- Raumtemperatur (per I2C vom DevKit, DS18B20 optional) ---
  // Getrennt benannt von den öffentlichen Radio::roomTempValid()/
  // roomTempC()-Funktionen weiter unten, um eine Namenskollision
  // zwischen Variable und gleichnamiger Funktion zu vermeiden.
  bool   roomTempOk = false;
  float  roomLastC = 0;
  unsigned long lastRoomPoll = 0;

  // --- Info-Zeile: rotiert alle INFO_LINE_ROTATE_MS zwischen
  // Raum/Aussentemperatur, Wetter heute und Wetter morgen ---
  int    infoMode = 0;  // 0=Raum+Aussen, 1=Heute, 2=Morgen
  unsigned long lastInfoRotate = 0;

  String fmtC(float c) {
    return String((int)roundf(c)) + "C";
  }

  // Wetter vereinfacht auf 4 Kategorien statt Icons -- reicht für eine
  // einzeilige Textanzeige.
  String weatherCategory(Weather::Icon icon) {
    using Icon = Weather::Icon;
    switch (icon) {
      case Icon::SUN:
      case Icon::PARTLY_CLOUDY: return "SCHOEN";
      case Icon::CLOUDY:
      case Icon::FOG:           return "BEWOELKT";
      case Icon::RAIN:
      case Icon::SNOW:          return "REGEN";
      case Icon::STORM:         return "STURM";
      default:                  return "n/a";
    }
  }

  String buildInfoLine() {
    if (infoMode == 0) {
      String room = roomTempOk ? fmtC(roomLastC) : "n/a";
      Weather::HourSlot wxNow = Weather::now();
      String outside = wxNow.valid ? fmtC(wxNow.tempC) : "n/a";
      return "RAUM " + room + "  DRAUSSEN " + outside;
    }
    Weather::DaySlot d = (infoMode == 1) ? Weather::today() : Weather::tomorrow();
    String label = (infoMode == 1) ? "HEUTE" : "MORGEN";
    if (!d.valid) return label + " n/a";
    return label + " " + weatherCategory(d.icon) + "  ~" + fmtC((d.tempMin + d.tempMax) / 2.0f);
  }

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
  Weather::loop();

  unsigned long now = millis();
  if (now - lastI2cPoll > I2C_POLL_INTERVAL_MS) {
    lastI2cPoll = now;
    uint8_t newStation;
    if (I2cMaster::pollButtons(newStation)) startStation(newStation);
  }

  if (now - lastRoomPoll > ROOM_TEMP_I2C_POLL_MS) {
    lastRoomPoll = now;
    float t; bool v;
    if (I2cMaster::getRoomTemp(t, v)) {
      roomTempOk = v;
      if (v) roomLastC = t;
    }
    // false (keine/unplausible Antwort) -> alten Stand beibehalten,
    // CLAUDE.md Stolperstein #6.
  }

  if (now - lastInfoRotate > INFO_LINE_ROTATE_MS) {
    lastInfoRotate = now;
    infoMode = (infoMode + 1) % 3;
  }
  // Display prüft intern auf tatsächliche Textänderung, bevor neu
  // gezeichnet wird -- unproblematisch, das jeden Loop-Durchlauf
  // neu zu berechnen.
  Display::setInfoLine(buildInfoLine());

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

bool roomTempValid() { return roomTempOk; }
float roomTempC() { return roomLastC; }

} // namespace Radio
