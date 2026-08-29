#include "storage.h"
#include <Preferences.h>

static Preferences prefs;

void storageLoadStations(Station stations[STATION_COUNT]) {
  prefs.begin("radio", true);
  bool hasData = prefs.isKey("n0");
  prefs.end();

  if (!hasData) {
    for (int i = 0; i < STATION_COUNT; i++) stations[i] = DEFAULT_STATIONS[i];
    storageSaveStations(stations);
    return;
  }

  prefs.begin("radio", true);
  for (int i = 0; i < STATION_COUNT; i++) {
    stations[i].name = prefs.getString(("n" + String(i)).c_str(), DEFAULT_STATIONS[i].name);
    stations[i].url  = prefs.getString(("u" + String(i)).c_str(), DEFAULT_STATIONS[i].url);
  }
  prefs.end();
}

void storageSaveStations(const Station stations[STATION_COUNT]) {
  prefs.begin("radio", false);
  for (int i = 0; i < STATION_COUNT; i++) {
    prefs.putString(("n" + String(i)).c_str(), stations[i].name);
    prefs.putString(("u" + String(i)).c_str(), stations[i].url);
  }
  prefs.end();
}

int storageLoadCurrentStation() {
  prefs.begin("radio", true);
  int idx = prefs.getInt("cur", 0);
  prefs.end();
  return constrain(idx, 0, STATION_COUNT - 1);
}

void storageSaveCurrentStation(int idx) {
  prefs.begin("radio", false);
  prefs.putInt("cur", idx);
  prefs.end();
}

bool storageLoadWifi(String &ssid, String &pass) {
  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();
  return ssid.length() > 0;
}

void storageSaveWifi(const String &ssid, const String &pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

void storageClearWifi() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
}
