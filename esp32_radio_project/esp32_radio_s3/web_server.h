#pragma once

// Retro-Monospace-Webinterface: Hauptseite (Status, Sender-Auswahl,
// -Konfiguration, WLAN-Reset, Neustart) + BT-Scan-Seite. Namespace heisst
// bewusst nicht "WebServer", um Namenskollision mit der Bibliotheksklasse
// WebServer aus <WebServer.h> zu vermeiden.
namespace RadioWeb {
  void begin();
  void loop();
}
