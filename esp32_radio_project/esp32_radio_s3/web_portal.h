// Captive Portal für WLAN-Ersteinrichtung (Stolperstein #7/#8 in CLAUDE.md).
// Blockierend: läuft bis Zugangsdaten gespeichert wurden oder Timeout.
#pragma once
#include <Arduino.h>

// Rückgabe true = neue Zugangsdaten wurden gespeichert (Neustart nötig).
bool webPortalRun();
