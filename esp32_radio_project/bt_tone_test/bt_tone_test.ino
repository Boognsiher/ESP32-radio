/*
 * Minimal-Testsketch: NUR Bluetooth-A2DP-Verbindungsstabilität testen,
 * mit einem lokal erzeugten Sinuston statt echtem Audio über I2S vom S3.
 * Isoliert die offene Frage aus der Fehlersuche (siehe Projekt-Memory
 * esp32-radio-audio-debug): die BT-Verbindung zum Lautsprecher bricht
 * nach kurzer Zeit ab, obwohl laut Diagnose echte Audiodaten ankommen --
 * dieser Sketch prüft, ob das Problem an der reinen A2DP-Verbindung
 * (unabhängig von WLAN-Stream/Decode/I2S) liegt.
 *
 * Verbindet sich mit demselben Ziel-Namen wie die normale Firmware
 * (liest ihn aus derselben NVS-Preferences "btname"/"name", damit kein
 * zusätzliches Setup nötig ist -- Fallback "UE BOOM", falls noch nichts
 * gespeichert ist).
 *
 * WICHTIG: eigener, separater Sketch-Ordner -- überschreibt NICHT die
 * normale Firmware in esp32_radio_bt/. Nach dem Test dort wieder
 * hinflashen, um zur normalen Funktion zurückzukehren.
 *
 * Gleiches Board-Package wie die Hauptfirmware: 2.0.x, Partitionsschema
 * "Huge APP" (siehe CLAUDE.md Stolperstein #2/#3, gilt hier genauso für
 * die ESP32-A2DP-Bibliothek).
 */

#include <BluetoothA2DPSource.h>
#include <Preferences.h>
#include <math.h>

BluetoothA2DPSource a2dp;
Preferences prefs;

// --- Sinuston-Erzeugung (Kammerton A, moderate Lautstärke) ---
constexpr float TONE_HZ     = 440.0f;
constexpr float SAMPLE_RATE = 44100.0f;
constexpr float PHASE_INC   = 2.0f * PI * TONE_HZ / SAMPLE_RATE;
float phase = 0.0f;

// --- Diagnose ---
volatile uint32_t callCount = 0;
volatile bool     connected = false;
unsigned long     connectedSince = 0;
unsigned long     lastStatusPrint = 0;

int32_t toneCallback(Frame *frame, int32_t frameCount) {
  callCount += frameCount;
  for (int32_t i = 0; i < frameCount; i++) {
    int16_t sample = (int16_t)(sinf(phase) * 8000.0f);
    phase += PHASE_INC;
    if (phase > 2.0f * PI) phase -= 2.0f * PI;
    frame[i].channel1 = sample;
    frame[i].channel2 = sample;
  }
  return frameCount;
}

void connectionChanged(esp_a2d_connection_state_t state, void *) {
  bool nowConnected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);
  if (nowConnected && !connected) connectedSince = millis();
  connected = nowConnected;
  Serial.printf("[BT] Verbindung: %s (state=%d)\n", connected ? "VERBUNDEN" : "GETRENNT", (int)state);
}

void audioStateChanged(esp_a2d_audio_state_t state, void *) {
  const char *names[] = {"Suspended(Remote)", "Started", "Suspended(Stopped)", "Suspended(Local)"};
  Serial.printf("[BT] Audio-Status: %s (%d)\n", (state >= 0 && state <= 3) ? names[state] : "?", (int)state);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[BT-TONE-TEST] Start -- reiner Verbindungsstabilitaets-Test mit Sinuston");

  prefs.begin("btname", true);
  String name = prefs.getString("name", "UE BOOM");
  prefs.end();
  Serial.printf("[BT-TONE-TEST] Ziel: %s\n", name.c_str());

  a2dp.set_data_callback_in_frames(toneCallback);
  // API-Name laut CLAUDE.md Stolperstein #4: set_on_connection_state_changed(),
  // nicht das ältere on_connection_state_changed().
  a2dp.set_on_connection_state_changed(connectionChanged, nullptr);
  a2dp.set_on_audio_state_changed(audioStateChanged, nullptr);
  a2dp.set_auto_reconnect(true);
  a2dp.start(name.c_str());
}

void loop() {
  if (millis() - lastStatusPrint > 3000) {
    lastStatusPrint = millis();
    unsigned long upSecs = connected ? (millis() - connectedSince) / 1000 : 0;
    Serial.printf("[BT-TONE-TEST] connected=%d upSince=%lus calls=%lu\n",
      connected, upSecs, (unsigned long)callCount);
  }
  delay(10);
}
