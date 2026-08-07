#pragma once
#include <Arduino.h>

// Wetterdaten von Open-Meteo (kostenlos, kein API-Key). Standort ist in
// NVS persistiert und über das Webinterface änderbar. HTTPS-Abruf ist
// hier unproblematisch (im Gegensatz zu Audio-Streams, siehe CLAUDE.md
// Stolperstein #9): es sind seltene, kleine JSON-Antworten, keine
// kontinuierliche Datenübertragung mit Knack-/Klick-Risiko.
namespace Weather {

  enum class Icon { SUN, PARTLY_CLOUDY, CLOUDY, FOG, RAIN, SNOW, STORM, UNKNOWN };

  struct HourSlot { float tempC; Icon icon; bool valid; };
  struct DaySlot  { float tempMin; float tempMax; Icon icon; bool valid; };

  // NTP-Zeitsynchronisation + Standort aus NVS laden. Erst nach WLAN-
  // Verbindungsaufbau aufrufen. Löst einen ersten Abruf aus.
  void begin();

  // In loop() aufrufen: löst periodisch (WEATHER_FETCH_INTERVAL_MS) einen
  // neuen Abruf aus. Der HTTPS-Request selbst blockiert kurz (siehe
  // WEATHER_HTTP_TIMEOUT_MS) -- bei einem alle 20 Minuten vernachlässigbar.
  void loop();

  // Sofortiger Abruf ausserhalb des normalen Intervalls (z.B. nach
  // Standortänderung im Webinterface).
  void fetchNow();

  void setLocation(float lat, float lon);  // speichert in NVS, löst fetchNow() aus
  float latitude();
  float longitude();

  bool lastFetchOk();

  // Aktuelle Werte bzw. leerer/invalider Slot, falls noch kein
  // erfolgreicher Abruf stattfand oder die lokale Uhrzeit (NTP) noch
  // nicht plausibel ist.
  HourSlot now();
  HourSlot plus6h();
  DaySlot  today();
  DaySlot  tomorrow();

  Icon codeToIcon(int wmoWeatherCode);
}
