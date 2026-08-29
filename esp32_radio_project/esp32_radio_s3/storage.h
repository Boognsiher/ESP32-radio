// NVS-Persistenz: WLAN-Zugangsdaten, Sender-Liste, letzter Sender.
#pragma once
#include <Arduino.h>
#include "config.h"

void storageLoadStations(Station stations[STATION_COUNT]);
void storageSaveStations(const Station stations[STATION_COUNT]);

int  storageLoadCurrentStation();
void storageSaveCurrentStation(int idx);

bool storageLoadWifi(String &ssid, String &pass);
void storageSaveWifi(const String &ssid, const String &pass);
void storageClearWifi();
