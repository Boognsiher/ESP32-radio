// Gemeinsamer Zustand, den mehrere Module lesen/anstossen müssen.
// Definiert im .ino, das Anzeige/Audio/Storage beim Sender-Wechsel
// koordiniert.
#pragma once
#include <Arduino.h>
#include "config.h"

extern Station stations[STATION_COUNT];
extern int     currentStation;
extern bool    wifiConnected;

// Wechselt sauber auf Sender idx: stoppt/startet Audio-Pipeline
// (Stolperstein #14), aktualisiert Display, persistiert die Auswahl.
void switchStation(int idx);
