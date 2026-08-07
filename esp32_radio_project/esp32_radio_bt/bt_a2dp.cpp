#include "bt_a2dp.h"
#include "config.h"
#include "i2s_audio.h"
#include <BluetoothA2DPSource.h>
#include <Preferences.h>
#include <string.h>

namespace {
  BluetoothA2DPSource a2dp;
  Preferences prefs;
  String name;
  volatile bool connected = false;

  void connectionChanged(esp_a2d_connection_state_t state, void *) {
    connected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);
    Serial.println(connected ? "[BT] Verbunden" : "[BT] Getrennt - suche neu...");
  }

  // Von der A2DP-Library aufgerufen, sobald neue Ausgabedaten benötigt
  // werden. Frame besteht aus zwei int16-Kanälen (L/R) -- entspricht
  // exakt dem interleaved-Layout, das I2sAudio::readFrames() liefert,
  // daher direkte Wiederverwendung des Zielpuffers ohne Zwischenkopie.
  int32_t dataCallback(Frame *frame, int32_t frameCount) {
    if (!connected) {
      memset(frame, 0, frameCount * sizeof(Frame));
      return frameCount;
    }
    size_t got = I2sAudio::readFrames(reinterpret_cast<int16_t *>(frame), (size_t)frameCount);
    for (size_t i = got; i < (size_t)frameCount; i++) {
      frame[i].channel1 = 0;
      frame[i].channel2 = 0;
    }
    return frameCount;
  }
}

namespace BtA2dp {

void begin() {
  prefs.begin("btname", true);
  name = prefs.getString("name", BT_DEFAULT_NAME);
  prefs.end();

  a2dp.set_auto_reconnect(true);
  // API-Name laut CLAUDE.md Stolperstein #4: set_on_connection_state_changed(),
  // nicht das ältere on_connection_state_changed().
  a2dp.set_on_connection_state_changed(connectionChanged, nullptr);
  a2dp.start(name.c_str(), dataCallback);
  Serial.printf("[BT] Verbinde mit: %s\n", name.c_str());
}

bool isConnected() { return connected; }
String deviceName() { return name; }

void setDeviceName(const String &newName) {
  prefs.begin("btname", false);
  prefs.putString("name", newName);
  prefs.end();
  Serial.printf("[BT] Neues Ziel gespeichert: %s -- Neustart...\n", newName.c_str());
  delay(800);
  ESP.restart();
}

} // namespace BtA2dp
