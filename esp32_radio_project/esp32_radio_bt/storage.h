// NVS-Persistenz: Ziel-Bluetooth-Lautsprechername.
#pragma once
#include <Arduino.h>

String storageLoadBtName();
void   storageSaveBtName(const String &name);
