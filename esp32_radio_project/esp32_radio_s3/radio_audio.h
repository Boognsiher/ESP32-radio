// Audio-Pipeline (arduino-audio-tools), siehe Stolperstein #10 in CLAUDE.md:
//   URLStream -> EncodedAudioStream(MP3DecoderHelix)
//             -> ResampleStream(->44.1kHz) -> I2SStream (Master, TX)
// Kein Eigenbau-I2S-Code mehr für die Audio-Übertragung selbst.
#pragma once
#include <Arduino.h>

void radioAudioInit();                    // I2S/Pipeline einmalig konfigurieren
void radioAudioStart(const String &url);  // sauberer Wechsel/Start (Stolperstein #14)
void radioAudioStop();
void radioAudioLoop();                    // in loop() aufrufen: kopiert Daten

bool   radioAudioIsPlaying();
String radioAudioStatusMsg();
String radioAudioCurrentTitle();
String radioAudioCurrentArtist();
