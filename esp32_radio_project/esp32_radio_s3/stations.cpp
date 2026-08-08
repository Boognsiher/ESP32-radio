#include "stations.h"
#include <Preferences.h>

namespace {
  Preferences prefs;
  Station stations[STATION_COUNT];

  // HTTP statt HTTPS bevorzugt (CLAUDE.md Stolperstein #9: HTTPS-Streams
  // können Klick-/Knack-Artefakte verursachen). Nur Vorbelegung -- über
  // das Webinterface frei überschreibbar.
  const Station DEFAULTS[STATION_COUNT] = {
    { "Swiss Jazz",      "http://stream.srg-ssr.ch/m/rsj/mp3_128" },
    { "Radio Swiss Pop", "http://stream.srg-ssr.ch/m/rsp/mp3_128" },
    { "SRF 3",           "http://stream.srg-ssr.ch/m/drs3/mp3_128" }
  };
}

namespace Stations {

void begin() {
  prefs.begin("radio", true);
  bool hasData = prefs.isKey("n0");
  prefs.end();

  if (!hasData) {
    for (int i = 0; i < STATION_COUNT; i++) stations[i] = DEFAULTS[i];
    save();
    return;
  }

  prefs.begin("radio", true);
  for (int i = 0; i < STATION_COUNT; i++) {
    stations[i].name = prefs.getString(("n" + String(i)).c_str(), DEFAULTS[i].name);
    stations[i].url  = prefs.getString(("u" + String(i)).c_str(), DEFAULTS[i].url);
  }
  prefs.end();
}

Station get(int idx) {
  if (idx < 0 || idx >= STATION_COUNT) idx = 0;
  return stations[idx];
}

void set(int idx, const String &name, const String &url) {
  if (idx < 0 || idx >= STATION_COUNT) return;
  stations[idx].name = name;
  stations[idx].url  = url;
}

void save() {
  prefs.begin("radio", false);
  for (int i = 0; i < STATION_COUNT; i++) {
    prefs.putString(("n" + String(i)).c_str(), stations[i].name);
    prefs.putString(("u" + String(i)).c_str(), stations[i].url);
  }
  prefs.end();
}

int loadCurrentIndex() {
  prefs.begin("radio", true);
  int idx = prefs.getInt("cur", 0);
  prefs.end();
  return constrain(idx, 0, STATION_COUNT - 1);
}

void saveCurrentIndex(int idx) {
  prefs.begin("radio", false);
  prefs.putInt("cur", idx);
  prefs.end();
}

} // namespace Stations
