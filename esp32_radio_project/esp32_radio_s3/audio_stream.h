#pragma once
#include <Arduino.h>

// Kapselt die ESP32-audioI2S-Bibliothek (schreibfaul2): dekodiert den
// MP3-Stream und gibt PCM per I2S an das DevKitV1 aus.
namespace AudioStream {
  typedef void (*TrackInfoCallback)(const String &artist, const String &title);
  typedef void (*StreamEndCallback)();

  void begin(int bclkPin, int lrckPin, int doutPin);
  void setCallbacks(TrackInfoCallback onTrack, StreamEndCallback onEnd);
  void play(const String &url);
  void stop();
  void loop();
  void setVolume(uint8_t vol);  // 0..21
}
