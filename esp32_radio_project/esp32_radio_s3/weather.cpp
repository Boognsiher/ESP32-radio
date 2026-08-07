#include "weather.h"
#include "config.h"
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

namespace {
  Preferences prefs;
  float lat = WEATHER_DEFAULT_LAT;
  float lon = WEATHER_DEFAULT_LON;

  Weather::HourSlot nowSlot     = { 0, Weather::Icon::UNKNOWN, false };
  Weather::HourSlot plus6Slot   = { 0, Weather::Icon::UNKNOWN, false };
  Weather::DaySlot  todaySlot   = { 0, 0, Weather::Icon::UNKNOWN, false };
  Weather::DaySlot  tomorrowSlot = { 0, 0, Weather::Icon::UNKNOWN, false };
  bool fetchOk = false;

  unsigned long lastFetchAt = 0;

  // Liefert die lokale Stunde (0..23) oder -1, falls die Systemzeit noch
  // nicht per NTP synchronisiert ist (Jahr < 2020 -> unplausibel, siehe
  // CLAUDE.md-Philosophie: unplausible Werte verwerfen statt verwenden).
  int currentLocalHour() {
    time_t t = time(nullptr);
    struct tm tmNow;
    localtime_r(&t, &tmNow);
    if (tmNow.tm_year + 1900 < 2020) return -1;
    return tmNow.tm_hour;
  }

  void loadLocation() {
    prefs.begin("weather", true);
    lat = prefs.getFloat("lat", WEATHER_DEFAULT_LAT);
    lon = prefs.getFloat("lon", WEATHER_DEFAULT_LON);
    prefs.end();
  }
}

namespace Weather {

Icon codeToIcon(int code) {
  if (code == 0) return Icon::SUN;
  if (code == 1) return Icon::PARTLY_CLOUDY;
  if (code == 2 || code == 3) return Icon::CLOUDY;
  if (code == 45 || code == 48) return Icon::FOG;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return Icon::RAIN;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return Icon::SNOW;
  if (code == 95 || code == 96 || code == 99) return Icon::STORM;
  return Icon::UNKNOWN;
}

void begin() {
  loadLocation();
  // CET/CEST mit automatischer Sommerzeitumstellung -- passend zu den
  // Schweizer Default-Sendern, per Standort aber ohnehin nur für die
  // Stunden-Indizierung relevant (Datum/Uhrzeit wird nirgends angezeigt).
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");
  fetchNow();
}

void setLocation(float newLat, float newLon) {
  lat = newLat;
  lon = newLon;
  prefs.begin("weather", false);
  prefs.putFloat("lat", lat);
  prefs.putFloat("lon", lon);
  prefs.end();
  fetchNow();
}

float latitude() { return lat; }
float longitude() { return lon; }
bool lastFetchOk() { return fetchOk; }

void fetchNow() {
  lastFetchAt = millis();
  if (WiFi.status() != WL_CONNECTED) { fetchOk = false; return; }

  WiFiClientSecure client;
  // Open-Meteo-Zertifikat wird nicht geprüft (kein CA-Bundle im Sketch
  // gepflegt) -- akzeptabel hier, da nur öffentliche, nicht-sensible
  // Wetterdaten abgerufen werden, keine Zugangsdaten o.ä. übertragen.
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(WEATHER_HTTP_TIMEOUT_MS);

  String url = String("https://") + WEATHER_API_HOST +
    "/v1/forecast?latitude=" + String(lat, 4) + "&longitude=" + String(lon, 4) +
    "&current=temperature_2m,weathercode"
    "&hourly=temperature_2m,weathercode"
    "&daily=weathercode,temperature_2m_max,temperature_2m_min"
    "&timezone=auto&forecast_days=2";

  if (!http.begin(client, url)) { fetchOk = false; return; }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[WETTER] HTTP-Fehler %d\n", code);
    http.end();
    fetchOk = false;
    return;
  }

  // Nur die tatsächlich benötigten Felder parsen (spart RAM -- v.a. die
  // "time"-Arrays mit ISO-Zeitstempeln pro Stunde/Tag werden übersprungen).
  StaticJsonDocument<512> filter;
  filter["current"]["temperature_2m"] = true;
  filter["current"]["weathercode"] = true;
  filter["hourly"]["temperature_2m"] = true;
  filter["hourly"]["weathercode"] = true;
  filter["daily"]["weathercode"] = true;
  filter["daily"]["temperature_2m_max"] = true;
  filter["daily"]["temperature_2m_min"] = true;

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, http.getStream(),
    DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    Serial.printf("[WETTER] JSON-Fehler: %s\n", err.c_str());
    fetchOk = false;
    return;
  }

  bool ok = true;

  if (doc["current"]["temperature_2m"].is<float>()) {
    nowSlot.tempC = doc["current"]["temperature_2m"].as<float>();
    nowSlot.icon = codeToIcon(doc["current"]["weathercode"] | -1);
    nowSlot.valid = true;
  } else {
    nowSlot.valid = false;
    ok = false;
  }

  // "Jetzt" kommt bewusst aus current (echtzeitnäher); hourly wird nur
  // für den "+6h"-Blick gebraucht, indiziert über die per NTP ermittelte
  // lokale Stunde (Open-Meteo richtet den hourly-Array-Index bei
  // timezone=auto an der lokalen Mitternacht von Tag 0 aus).
  JsonArray hTemp = doc["hourly"]["temperature_2m"];
  JsonArray hCode = doc["hourly"]["weathercode"];
  int hour = currentLocalHour();
  int idx6 = (hour >= 0) ? hour + 6 : -1;
  if (idx6 >= 0 && idx6 < (int)hTemp.size() && idx6 < (int)hCode.size()) {
    plus6Slot.tempC = hTemp[idx6].as<float>();
    plus6Slot.icon = codeToIcon(hCode[idx6].as<int>());
    plus6Slot.valid = true;
  } else {
    plus6Slot.valid = false;  // NTP noch nicht synchron oder Array zu kurz
  }

  JsonArray dCode = doc["daily"]["weathercode"];
  JsonArray dMax = doc["daily"]["temperature_2m_max"];
  JsonArray dMin = doc["daily"]["temperature_2m_min"];

  if (dCode.size() >= 1 && dMax.size() >= 1 && dMin.size() >= 1) {
    todaySlot = { dMin[0].as<float>(), dMax[0].as<float>(), codeToIcon(dCode[0].as<int>()), true };
  } else {
    todaySlot.valid = false;
    ok = false;
  }

  if (dCode.size() >= 2 && dMax.size() >= 2 && dMin.size() >= 2) {
    tomorrowSlot = { dMin[1].as<float>(), dMax[1].as<float>(), codeToIcon(dCode[1].as<int>()), true };
  } else {
    tomorrowSlot.valid = false;
  }

  fetchOk = ok;
  Serial.printf("[WETTER] Abruf %s (jetzt=%.1f°C)\n", ok ? "ok" : "unvollstaendig", nowSlot.tempC);
}

void loop() {
  if (millis() - lastFetchAt >= WEATHER_FETCH_INTERVAL_MS) fetchNow();
}

HourSlot now()      { return nowSlot; }
HourSlot plus6h()    { return plus6Slot; }
DaySlot  today()      { return todaySlot; }
DaySlot  tomorrow()    { return tomorrowSlot; }

} // namespace Weather
