#include "audio_stream.h"
#include <Audio.h>   // ESP32-audioI2S (schreibfaul2)

namespace {
  Audio audio;
  AudioStream::TrackInfoCallback trackCb = nullptr;
  AudioStream::StreamEndCallback endCb   = nullptr;
}

// Die Audio-Library ruft diese global benannten Funktionen selbst auf
// (kein Registrierungsmechanismus, feste Namen) -- siehe
// reference/esp32_radio_s3_OLD.ino für den bestätigt funktionierenden
// Funktionsnamen/-signaturen.
void audio_showstreamtitle(const char *info) {
  String s(info);
  s.trim();
  int sep = s.indexOf(" - ");
  String artist = sep > 0 ? s.substring(0, sep) : "";
  String title  = sep > 0 ? s.substring(sep + 3) : s;
  if (trackCb) trackCb(artist, title);
}

void audio_eof_mp3(const char *info) {
  if (endCb) endCb();
}

namespace AudioStream {

void begin(int bclkPin, int lrckPin, int doutPin) {
  audio.setPinout(bclkPin, lrckPin, doutPin);
  audio.setVolume(17);
}

void setCallbacks(TrackInfoCallback onTrack, StreamEndCallback onEnd) {
  trackCb = onTrack;
  endCb   = onEnd;
}

void play(const String &url) {
  audio.stopSong();
  delay(200);
  audio.connecttohost(url.c_str());
}

void stop() {
  audio.stopSong();
}

void loop() {
  audio.loop();
}

void setVolume(uint8_t vol) {
  audio.setVolume(vol);
}

} // namespace AudioStream
