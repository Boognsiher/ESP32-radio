#include "bt_a2dp.h"
#include "config.h"
#include "i2s_audio.h"
#include <BluetoothA2DPSource.h>
#include <Preferences.h>
#include <esp_bt.h>
#include <string.h>
#include <stdio.h>

namespace {
  BluetoothA2DPSource a2dp;
  Preferences prefs;
  String  name;
  uint8_t targetMac[6] = { 0, 0, 0, 0, 0, 0 };
  bool    macMode = false;
  volatile bool connected = false;

  // --- Temporäre Diagnose: kommen über I2S überhaupt reale (nicht-stille)
  // Samples vom S3 an? (Fehlersuche "verbunden + ON AIR, aber kein Ton")
  volatile uint32_t dbgCalls = 0, dbgZeroReads = 0;
  volatile int16_t  dbgMaxAbs = 0;

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

  // Temporäre Diagnose: die AVDTP-Signalisierungsverbindung (oben,
  // "verbunden") ist unabhängig vom eigentlichen Audio-STREAMING-Zustand
  // (Suspended/Started) -- Fehlersuche "verbunden + reale Samples, aber
  // kein Ton" könnte hier hängen bleiben, falls die Media-Session nie
  // "Started" erreicht.
  void audioStateChanged(esp_a2d_audio_state_t state, void *) {
    const char *names[] = {"Suspended(Remote)", "Started", "Suspended(Stopped)", "Suspended(Local)"};
    Serial.printf("[BT] Audio-Streaming-Status: %s (%d)\n",
      (state >= 0 && state <= 3) ? names[state] : "?", (int)state);
  }

  // Von der A2DP-Library aufgerufen, sobald neue Ausgabedaten benötigt
  // werden. Frame besteht aus zwei int16-Kanälen (L/R) -- entspricht
  // exakt dem interleaved-Layout, das I2sAudio::readFrames() liefert,
  // daher direkte Wiederverwendung des Zielpuffers ohne Zwischenkopie.
  int32_t dataCallback(Frame *frame, int32_t frameCount) {
    dbgCalls++;
    if (!connected) {
      memset(frame, 0, frameCount * sizeof(Frame));
      return frameCount;
    }
    size_t got = I2sAudio::readFrames(reinterpret_cast<int16_t *>(frame), (size_t)frameCount);
    if (got == 0) dbgZeroReads++;
    int16_t *samples = reinterpret_cast<int16_t *>(frame);
    for (size_t i = 0; i < got * 2; i++) {
      int16_t a = samples[i] < 0 ? -samples[i] : samples[i];
      if (a > dbgMaxAbs) dbgMaxAbs = a;
    }
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
  a2dp.set_on_audio_state_changed(audioStateChanged, nullptr);

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

  // BT-Sendeleistung -- muss NACH a2dp.start() erfolgen (start() initialisiert
  // den BT-Controller synchron; vorher hat esp_bredr_tx_power_set() nichts zu
  // setzen). Hardware-Test-Feedback: periodische Verbindungsabbrueche
  // (~alle 15-20s) traten unabhaengig von WLAN-Sendeleistung UND Abstand
  // Lautsprecher/Board auf -- einzig die BT-Sende-UNTERGRENZE entschied
  // zuverlaessig (N9/N0 durchgehend stabil ueber 60s+, N3/N0 und Standard
  // N0/P3 brachen beide weiterhin ab). Bei erstem N9/N0-Test (Lautsprecher
  // nah am Board) klang der Ton "abgehackt" -- vermutlich ein Nahfeld-
  // Artefakt bei geringer Sendeleistung + geringem Abstand, nicht
  // zwingend an die niedrige Leistung selbst gebunden.
  esp_bredr_tx_power_set(ESP_PWR_LVL_N9, ESP_PWR_LVL_N0);
}

bool isConnected() { return connected; }
String deviceName() { return name; }

// Temporäre Diagnose (siehe dbgCalls/dbgZeroReads/dbgMaxAbs oben) --
// zeigt, ob dataCallback() überhaupt läuft, ob I2sAudio::readFrames()
// leer zurückkommt (0 Frames = liest nichts) und ob echte, nicht-stille
// Samples ankommen (dbgMaxAbs bleibt 0 -> nur Stille/kein Signal).
void printAudioDebug() {
  Serial.printf("[AUDIODBG] calls=%lu zeroReads=%lu maxAbsSample=%d\n",
    (unsigned long)dbgCalls, (unsigned long)dbgZeroReads, dbgMaxAbs);
  dbgCalls = 0; dbgZeroReads = 0; dbgMaxAbs = 0;
}

void disconnect() {
  Serial.println("[BT] Trenne aktiv (fuer Scan)...");
  a2dp.disconnect();
}

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
