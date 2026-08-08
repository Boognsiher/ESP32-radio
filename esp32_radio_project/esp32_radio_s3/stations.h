#pragma once
#include <Arduino.h>
#include "config.h"

struct Station {
  String name;
  String url;
};

// Persistiert die 3 Sender (Name+URL) sowie den zuletzt gehörten Sender
// in NVS (Preferences-Namespace "radio").
namespace Stations {
  void begin();                    // lädt aus NVS oder legt Default-Sender an
  Station get(int idx);
  void set(int idx, const String &name, const String &url);
  void save();

  int  loadCurrentIndex();
  void saveCurrentIndex(int idx);
}
