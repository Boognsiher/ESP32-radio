#pragma once
#include <Arduino.h>

// Optionaler DS18B20-Raumtemperatursensor (1-Wire) an PIN_ROOM_TEMP_SENSOR.
// Nicht-blockierend: requestTemperatures() + Auslesen laufen über einen
// Zustandsautomat in loop(), damit die ~750ms DS18B20-Wandlungszeit nicht
// den restlichen loop() (Taster, I2C, Serial) blockiert. Kein Sensor
// angeschlossen -> isValid() bleibt dauerhaft false, kein Fehlerzustand.
namespace RoomSensor {
  void begin();
  void loop();

  bool isValid();
  float getCelsius();  // nur gültig, wenn isValid() true ist
}
