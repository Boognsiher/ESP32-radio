// Hauptwebinterface (Retro-Monospace-Stil): Status, Sender-Konfiguration,
// WLAN-Reset, Neustart, BT-Geräte-Scan mit Verbinden-Funktion.
#pragma once

void webUiStart();   // registriert Routen + server.begin()
void webUiLoop();    // in loop() aufrufen
