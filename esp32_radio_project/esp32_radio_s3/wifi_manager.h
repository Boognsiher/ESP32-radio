#pragma once
#include <Arduino.h>

// WLAN-Verbindungsaufbau mit gespeicherten Zugangsdaten (NVS) und
// Captive-Portal-Fallback (CLAUDE.md Stolperstein #7/#8).
namespace WifiManager {
  typedef void (*StatusCallback)(const String &line1, const String &line2);

  bool hasStoredCredentials();
  void clearCredentials();

  // Verbindet mit gespeicherten Zugangsdaten (statische IP aus config.h).
  // Blockiert bis Erfolg oder WIFI_CONNECT_TIMEOUT_MS erreicht ist.
  bool connectStored(StatusCallback onStatus);

  // Öffnet den Captive-Portal-Hotspot zur Ersteinrichtung/Korrektur.
  // Blockiert bis neue Zugangsdaten gespeichert wurden oder
  // CAPTIVE_PORTAL_TIMEOUT_MS erreicht ist. Gibt true zurück, wenn neue
  // Daten gespeichert wurden.
  bool runCaptivePortal(StatusCallback onStatus);
}
