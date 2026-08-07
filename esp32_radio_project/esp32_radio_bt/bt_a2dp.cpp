#include "bt_a2dp.h"
#include "config.h"
#include "i2s_audio.h"
#include <BluetoothA2DPSource.h>
#include <Preferences.h>
#include <string.h>
#include <stdio.h>

namespace {
  BluetoothA2DPSource a2dp;
  Preferences prefs;
  String  name;
  uint8_t targetMac[6] = { 0, 0, 0, 0, 0, 0 };
  bool    macMode = false;
  volatile bool connected = false;

  String macToStr(const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
  }

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
  macMode = prefs.isKey("mac") && prefs.getBytesLength("mac") == 6;
  if (macMode) prefs.getBytes("mac", targetMac, 6);
  prefs.end();

  a2dp.set_data_callback_in_frames(dataCallback);
  // API-Name laut CLAUDE.md Stolperstein #4: set_on_connection_state_changed(),
  // nicht das ältere on_connection_state_changed().
  a2dp.set_on_connection_state_changed(connectionChanged, nullptr);

  if (macMode) {
    // set_auto_reconnect(addr, retries) hinterlegt die Adresse als
    // "last_connection"; start() ohne Namen verbindet laut Library-
    // Quellcode dann direkt dorthin, ohne vorherigen Discovery-Scan --
    // deutlich zuverlässiger als die Namenssuche unten.
    a2dp.set_auto_reconnect(targetMac, BT_MAC_RECONNECT_RETRIES);
    Serial.printf("[BT] Verbinde per fester MAC: %s\n", macToStr(targetMac).c_str());
    a2dp.start();
  } else {
    a2dp.set_auto_reconnect(true);
    Serial.printf("[BT] Verbinde per Name: %s\n", name.c_str());
    a2dp.start(name.c_str());
  }
}

bool isConnected() { return connected; }
String deviceName() { return name; }

String targetLabel() {
  return macMode ? ("MAC " + macToStr(targetMac)) : name;
}

void setDeviceName(const String &newName) {
  prefs.begin("btname", false);
  prefs.putString("name", newName);
  prefs.end();
  Serial.printf("[BT] Neues Ziel gespeichert: %s -- Neustart...\n", newName.c_str());
  delay(800);
  ESP.restart();
}

void setDeviceMac(const uint8_t mac[6]) {
  prefs.begin("btname", false);
  prefs.putBytes("mac", mac, 6);
  prefs.end();
  Serial.printf("[BT] Neue Ziel-MAC gespeichert: %s -- Neustart...\n", macToStr(mac).c_str());
  delay(800);
  ESP.restart();
}

void clearDeviceMac() {
  prefs.begin("btname", false);
  prefs.remove("mac");
  prefs.end();
  Serial.println("[BT] Feste MAC entfernt -- Neustart (Namens-Modus)...");
  delay(800);
  ESP.restart();
}

} // namespace BtA2dp
