#include "storage.h"
#include "config.h"
#include <Preferences.h>

static Preferences prefs;

String storageLoadBtName() {
  prefs.begin("btname", true);
  String name = prefs.getString("name", BT_NAME_DEFAULT);
  prefs.end();
  return name;
}

void storageSaveBtName(const String &name) {
  prefs.begin("btname", false);
  prefs.putString("name", name);
  prefs.end();
}
