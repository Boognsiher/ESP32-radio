#pragma once
#include <Arduino.h>

// I2S-Empfang (Slave/RX) vom Xiao S3 -- liefert PCM-Frames für die
// A2DP-Datenquelle in bt_a2dp.cpp.
namespace I2sAudio {
  void begin();

  // Liest bis zu frameCount Stereo-Frames (interleaved int16 L/R) in
  // outLR. Gibt die Anzahl tatsächlich gelesener Frames zurück (kann
  // kleiner als frameCount sein).
  size_t readFrames(int16_t *outLR, size_t frameCount);
}
