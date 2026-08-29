// Taster-Einlesen mit Entprellung + Event-Zähler für I2C-Abfrage.
#pragma once
#include <Arduino.h>

void buttonsInit();
void buttonsLoop();   // in loop() aufrufen

// Fuer den I2C-Slave: volatile, da im I2C-Callback (ISR-Kontext) gelesen.
extern volatile uint8_t buttonCurrentStation;   // 0..STATION_COUNT-1
extern volatile uint8_t buttonEventCounter;     // erhoeht sich bei jedem Tastendruck
