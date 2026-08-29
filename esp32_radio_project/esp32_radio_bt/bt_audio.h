// Audio-Pipeline (arduino-audio-tools), siehe Stolperstein #10 in CLAUDE.md:
//   I2SStream (Slave, RX) -> A2DPStream (TX) -> Bluetooth-Lautsprecher
// Kein Eigenbau-I2S-/A2DP-Code mehr (kein driver/i2s.h, keine eigene
// BluetoothA2DPSource-Einbindung).
#pragma once
#include <Arduino.h>

void btAudioInit(const String &btDeviceName);
void btAudioLoop();          // in loop() aufrufen: kopiert I2S->A2DP
bool btAudioIsConnected();
