#include "serial_console.h"
#include "bt_a2dp.h"
#include "bt_scan.h"
#include "buttons.h"
#include <Arduino.h>
#include <stdlib.h>

namespace {
  // Parst "AA:BB:CC:DD:EE:FF" (gross-/kleinschreibungsunabhängig) in 6
  // Rohbytes. Gibt false bei ungültigem Format zurück.
  bool parseMacAddress(const String &text, uint8_t out[6]) {
    if (text.length() != 17) return false;
    for (int i = 0; i < 6; i++) {
      if (i < 5 && text[i * 3 + 2] != ':') return false;
      char hex[3] = { text[i * 3], text[i * 3 + 1], 0 };
      char *end = nullptr;
      long v = strtol(hex, &end, 16);
      if (end != hex + 2) return false;
      out[i] = (uint8_t)v;
    }
    return true;
  }
}

namespace SerialConsole {

void poll() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.startsWith("setbtmac:")) {
    String macStr = cmd.substring(9);
    macStr.trim();
    uint8_t mac[6];
    if (parseMacAddress(macStr, mac)) {
      BtA2dp::setDeviceMac(mac);  // führt intern ESP.restart() aus
    } else {
      Serial.println("[FEHLER] Ungueltige MAC-Adresse (Format AA:BB:CC:DD:EE:FF)");
    }
  } else if (cmd == "clearbtmac") {
    BtA2dp::clearDeviceMac();  // führt intern ESP.restart() aus
  } else if (cmd.startsWith("setbt:")) {
    String name = cmd.substring(6);
    name.trim();
    if (name.length() > 0) BtA2dp::setDeviceName(name);  // führt intern ESP.restart() aus
  } else if (cmd == "status") {
    Serial.printf("[STATUS] Ziel: %s | Verbunden: %s | Sender: %d\n",
      BtA2dp::targetLabel().c_str(), BtA2dp::isConnected() ? "JA" : "NEIN",
      Buttons::currentStation() + 1);
  } else if (cmd == "scan") {
    BtScan::start();
  }
}

} // namespace SerialConsole
