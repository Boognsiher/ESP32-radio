#include "room_sensor.h"
#include "config.h"
#include <OneWire.h>
#include <DallasTemperature.h>

namespace {
  OneWire oneWire(PIN_ROOM_TEMP_SENSOR);
  DallasTemperature sensors(&oneWire);

  enum State { IDLE, WAITING };
  State state = IDLE;

  bool  sensorFound = false;
  bool  lastValid = false;
  float lastCelsius = 0.0f;
  unsigned long stateChangedAt = 0;

  // Wandlungszeit bei DS18B20-Standardauflösung (12 Bit).
  const unsigned long CONVERSION_MS = 750;
}

namespace RoomSensor {

void begin() {
  sensors.begin();
  sensors.setWaitForConversion(false);  // asynchron: requestTemperatures() blockiert nicht
  sensorFound = (sensors.getDeviceCount() > 0);
  Serial.println(sensorFound ? "[TEMP] DS18B20 gefunden"
                              : "[TEMP] Kein DS18B20 gefunden (optional, wird ignoriert)");
  // Unsigned-Unterlauf ist hier beabsichtigt: sorgt dafür, dass die erste
  // Messung gleich beim nächsten loop()-Aufruf startet statt erst nach
  // ROOM_TEMP_POLL_INTERVAL_MS.
  stateChangedAt = millis() - ROOM_TEMP_POLL_INTERVAL_MS;
}

void loop() {
  if (!sensorFound) return;
  unsigned long now = millis();

  if (state == IDLE) {
    if (now - stateChangedAt >= ROOM_TEMP_POLL_INTERVAL_MS) {
      sensors.requestTemperatures();
      state = WAITING;
      stateChangedAt = now;
    }
  } else {  // WAITING
    if (now - stateChangedAt >= CONVERSION_MS) {
      float c = sensors.getTempCByIndex(0);
      if (c != DEVICE_DISCONNECTED_C) {
        lastCelsius = c;
        lastValid = true;
      } else {
        lastValid = false;
        Serial.println("[TEMP] Lesefehler (Sensor getrennt?)");
      }
      state = IDLE;
      stateChangedAt = now;
    }
  }
}

bool isValid() { return sensorFound && lastValid; }
float getCelsius() { return lastCelsius; }

} // namespace RoomSensor
