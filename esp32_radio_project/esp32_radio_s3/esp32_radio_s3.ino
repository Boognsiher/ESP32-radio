/*
 * Xiao ESP32-S3 -- Internetradio
 * ===========================================================================
 * WLAN-Streaming von 3 konfigurierbaren Sendern, Ausgabe per I2S an das
 * ESP32 DevKitV1 (Bluetooth-A2DP-Bridge), rundes ST77916-QSPI-Display im
 * Phosphor-Grün-Retro-Stil, Retro-Webinterface (Sender-Konfiguration,
 * Captive-Portal-WLAN-Setup, BT-Geräte-Scan). Sender-Wechsel per Taster
 * kommt über I2C vom DevKitV1 (siehe CLAUDE.md: dort sitzen die Taster,
 * weil am S3 keine freien Pins mehr sind).
 *
 * Zusätzlich: Wetter-Anzeige (Open-Meteo, Standort im Webinterface
 * einstellbar) und Raumtemperatur (optionaler DS18B20 am DevKit, per
 * I2C durchgereicht) -- Display wechselt periodisch zwischen Radio- und
 * Wetter-Ansicht (siehe radio.cpp).
 *
 * Vollständige Spezifikation: ../CLAUDE.md
 * Verkabelung: ../hardware/pinout.md
 *
 * Benötigte Libraries (Board-Package **2.0.x**, Partitionsschema
 * "Huge APP" -- siehe CLAUDE.md Stolperstein #2/#3):
 *   - "GFX Library for Arduino" von moononournation
 *   - "ESP32-audioI2S" von schreibfaul2
 *   - "ArduinoJson" von bblanchon (Version 6.x)
 *   (WiFi/WebServer/DNSServer/ESPmDNS/Preferences/Wire/HTTPClient/
 *   WiFiClientSecure sind Teil des ESP32-Board-Packages)
 */

#include "config.h"
#include "display.h"
#include "stations.h"
#include "wifi_manager.h"
#include "audio_stream.h"
#include "i2c_master.h"
#include "weather.h"
#include "radio.h"
#include "web_server.h"

void setup() {
  Serial.begin(115200);
  delay(300);

  Display::begin();
  Display::showMessage("RADIO", "S3 Round");
  delay(1200);

  I2cMaster::begin();
  AudioStream::begin(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT);
  Radio::begin();

  bool hadCredentials = WifiManager::hasStoredCredentials();
  bool connected = hadCredentials && WifiManager::connectStored(Display::showMessage);

  if (!connected) {
    // Kein gespeichertes WLAN ODER falsches Passwort (CLAUDE.md
    // Stolperstein #8): in beiden Fällen automatisch den Hotspot zur
    // (Neu-)Eingabe öffnen, kein Reflash/Serial-Befehl nötig.
    Display::showMessage(hadCredentials ? "WLAN FEHLER" : "KEIN WLAN", "SETUP NOETIG");
    delay(1200);
    WifiManager::runCaptivePortal(Display::showMessage);
    ESP.restart();  // sauberer Neustart, egal ob neue Daten gespeichert wurden oder nicht
  }

  Radio::setWifiConnected(true);
  Weather::begin();
  RadioWeb::begin();
  Radio::startStation(Stations::loadCurrentIndex());
}

void loop() {
  RadioWeb::loop();
  Radio::loop();
}
