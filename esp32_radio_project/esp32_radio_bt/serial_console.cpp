#include "serial_console.h"
#include "bt_a2dp.h"
#include "bt_scan.h"
#include "buttons.h"
#include <Arduino.h>

namespace SerialConsole {

void poll() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.startsWith("setbt:")) {
    String name = cmd.substring(6);
    name.trim();
    if (name.length() > 0) BtA2dp::setDeviceName(name);  // führt intern ESP.restart() aus
  } else if (cmd == "status") {
    Serial.printf("[STATUS] Geraet: %s | Verbunden: %s | Sender: %d\n",
      BtA2dp::deviceName().c_str(), BtA2dp::isConnected() ? "JA" : "NEIN",
      Buttons::currentStation() + 1);
  } else if (cmd == "scan") {
    BtScan::start();
  }
}

} // namespace SerialConsole
