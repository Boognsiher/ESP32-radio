/*
 * Xiao ESP32-S3 - Internet Radio (rundes ST77916 QSPI-Display, 360x360)
 * ============================================================================
 * WLAN-Streaming, Webinterface, Display. Audio-Pipeline und BT-Bridge-
 * Details siehe CLAUDE.md (verbindliche Spezifikation) und die einzelnen
 * Module in diesem Sketch-Ordner. Pinbelegung siehe hardware/pinout.md.
 *
 * Zweiter Anlauf (siehe CLAUDE.md "Zweiter Anlauf"): Audio-Pipeline baut
 * auf arduino-audio-tools statt Eigenbau-I2S-Code auf.
 *
 * Benötigte Libraries (Library Manager):
 *   - "GFX Library for Arduino" (moononournation)
 *   - "arduino-audio-tools" (pschatzmann)
 *   - "arduino-libhelix" (pschatzmann) - MP3-Decoder-Abhängigkeit
 *   - "ESP32-A2DP" (pschatzmann) - Abhängigkeit von arduino-audio-tools
 */

#include "config.h"
#include "storage.h"
#include "display.h"
#include "web_portal.h"
#include "web_ui.h"
#include "radio_audio.h"
#include "i2c_link.h"
#include "radio_state.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Wire.h>

Station stations[STATION_COUNT];
int     currentStation = 0;
bool    wifiConnected  = false;

static unsigned long lastScrollMs  = 0;
static unsigned long lastStatusMs  = 0;
static unsigned long lastI2cPollMs = 0;

void switchStation(int idx) {
  currentStation = idx;
  radioAudioStart(stations[currentStation].url);
  storageSaveCurrentStation(currentStation);

  String names[STATION_COUNT];
  for (int i = 0; i < STATION_COUNT; i++) names[i] = stations[i].name;
  displayFullUpdate(currentStation, names, wifiConnected);
}

static void onButtonStationChanged(int idx) {
  switchStation(idx);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  i2cLinkInit();
  displayInit();
  displayMessage("RADIO", "S3 Round  v6.0");
  delay(1500);

  storageLoadStations(stations);
  currentStation = storageLoadCurrentStation();

  String ssid, pass;
  if (!storageLoadWifi(ssid, pass)) {
    bool ok = webPortalRun();
    if (ok) ESP.restart();
    displayMessage("KEIN WLAN", "NEUSTART NOETIG");
    while (true) delay(1000);
  }

  displayMessage("WLAN...", ssid);

  IPAddress ip, gw, sn, dns1;
  ip.fromString(STATIC_IP);
  gw.fromString(GATEWAY);
  sn.fromString(SUBNET);
  dns1.fromString(DNS_SERVER);
  WiFi.config(ip, gw, sn, dns1);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  for (int i = 0; WiFi.status() != WL_CONNECTED && i < WIFI_CONNECT_TIMEOUT_TRIES; i++) {
    delay(500);
    String dots = "";
    for (int d = 0; d < (i % 4); d++) dots += ".";
    displayMessage("VERBINDE" + dots, ssid);
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("[WiFi] %s\n", WiFi.localIP().toString().c_str());
    if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);

    radioAudioInit();
    webUiStart();
    switchStation(currentStation);
  } else {
    // Verbindung fehlgeschlagen (z.B. falsches Passwort, Stolperstein #8)
    // -> automatisch Captive Portal zur Neueingabe oeffnen, kein Reflash noetig.
    wifiConnected = false;
    displayMessage("WLAN FEHLER", "NEUE EINGABE...");
    delay(1500);
    bool ok = webPortalRun();
    ESP.restart();  // sowohl bei erfolgreicher Neueingabe als auch bei Timeout
  }
}

void loop() {
  webUiLoop();
  radioAudioLoop();

  unsigned long now = millis();

  if (now - lastScrollMs > 350) {
    lastScrollMs = now;
    displayUpdateTitleLine(radioAudioCurrentTitle(), radioAudioIsPlaying(), radioAudioStatusMsg());
  }

  // Blinkendes ON AIR + IP alle 800ms - nur diese Zeile neu zeichnen
  // (Stolperstein #5: kein Vollbild-Refresh, verhindert Flackern).
  if (now - lastStatusMs > 800) {
    lastStatusMs = now;
    displayUpdateStatusLine(radioAudioIsPlaying(), radioAudioStatusMsg(), wifiConnected, WiFi.localIP().toString());
  }

  if (now - lastI2cPollMs > I2C_POLL_MS) {
    lastI2cPollMs = now;
    i2cLinkPollButtons(onButtonStationChanged);
  }

  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    radioAudioStop();
  }

  if (!wifiConnected && WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    delay(1000);
    switchStation(currentStation);
  }

  delay(5);
}
