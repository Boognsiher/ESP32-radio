// ST77916 rundes QSPI-Display (360x360), Phosphor-Grün Retro-UI.
// Konstruktor/Init-Sequenz 1:1 aus dem bekannt funktionierenden Referenz-
// Sketch übernommen (siehe Stolperstein #1 in CLAUDE.md) – ohne die
// st77916_150_init_operations-Sequenz zeigt das Display nur Bildreste.
#pragma once
#include <Arduino.h>

void displayInit();
void displayMessage(const String &line1, const String &line2 = "");

// Vollbild-Neuzeichnen: nur bei Sender-/WLAN-Wechsel aufrufen (Stolperstein #5).
void displayFullUpdate(int currentStation, const String stationNames[], bool wifiConnected);

// Leichte Teil-Updates, verhindern Flackern bei häufigen Aktualisierungen.
void displayUpdateTitleLine(const String &title, bool isPlaying, const String &statusMsg);
void displayUpdateStatusLine(bool isPlaying, const String &statusMsg, bool wifiConnected, const String &ip);
