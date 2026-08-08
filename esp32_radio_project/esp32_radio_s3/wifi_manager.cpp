#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

namespace {
  Preferences prefs;

  const char *PAGE_HEAD =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>body{font-family:monospace;background:#050505;color:#00cc44;"
    "padding:20px;max-width:400px;margin:auto}"
    "h1{color:#00ff55}h2{color:#00aa33}"
    "label{display:block;color:#008833;margin-top:12px;font-size:12px}"
    "input{width:100%;padding:10px;background:#030d06;border:1px solid #003311;"
    "color:#00ff55;font-family:monospace;border-radius:2px;box-sizing:border-box;margin-top:4px}"
    "button{width:100%;padding:12px;background:#001a09;border:1px solid #00cc44;"
    "color:#00ff55;font-family:monospace;font-weight:bold;cursor:pointer;"
    "margin-top:14px;letter-spacing:2px}</style></head><body>";
}

namespace WifiManager {

bool hasStoredCredentials() {
  prefs.begin("wifi", true);
  bool has = prefs.getString("ssid", "").length() > 0;
  prefs.end();
  return has;
}

void clearCredentials() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
}

bool connectStored(StatusCallback onStatus) {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();
  if (ssid.length() == 0) return false;

  if (onStatus) onStatus("WLAN...", ssid);

  IPAddress ip, gw, sn, dns1;
  ip.fromString(WIFI_STATIC_IP);
  gw.fromString(WIFI_GATEWAY);
  sn.fromString(WIFI_SUBNET);
  dns1.fromString(WIFI_DNS);
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_TX_POWER_LEVEL);
  WiFi.config(ip, gw, sn, dns1);
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  int dotCount = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    if (onStatus) {
      String dots;
      for (int i = 0; i < (dotCount++ % 4); i++) dots += ".";
      onStatus("VERBINDE" + dots, ssid);
    }
  }
  return WiFi.status() == WL_CONNECTED;
}

bool runCaptivePortal(StatusCallback onStatus) {
  if (onStatus) onStatus("WLAN SETUP", WIFI_AP_SSID);

  WebServer portalServer(80);
  DNSServer dns;
  bool configured = false;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
  dns.start(53, "*", IPAddress(192, 168, 4, 1));

  portalServer.on("/", HTTP_GET, [&]() {
    String html = String(PAGE_HEAD) +
      "<h1>>> ESP32 RADIO</h1><h2>WLAN SETUP</h2>"
      "<form method='POST' action='/savewifi'>"
      "<label>NETZWERK (SSID)</label><input name='ssid' required>"
      "<label>PASSWORT</label><input type='password' name='pass'>"
      "<button>// VERBINDEN</button></form></body></html>";
    portalServer.send(200, "text/html", html);
  });

  portalServer.on("/savewifi", HTTP_POST, [&]() {
    String ssid = portalServer.arg("ssid");
    String pass = portalServer.arg("pass");
    if (ssid.length() > 0) {
      prefs.begin("wifi", false);
      prefs.putString("ssid", ssid);
      prefs.putString("pass", pass);
      prefs.end();
      portalServer.send(200, "text/html",
        String(PAGE_HEAD) + "<h2>>> GESPEICHERT - NEUSTART...</h2></body></html>");
      configured = true;
    }
  });

  portalServer.onNotFound([&]() {
    portalServer.sendHeader("Location", "http://192.168.4.1/");
    portalServer.send(302);
  });

  portalServer.begin();
  unsigned long timeout = millis() + CAPTIVE_PORTAL_TIMEOUT_MS;
  while (!configured && millis() < timeout) {
    dns.processNextRequest();
    portalServer.handleClient();
    delay(10);
  }
  // Kurze Gnadenfrist, damit die Antwort der /savewifi-Seite noch beim
  // Client ankommt, bevor der Hotspot verschwindet.
  if (configured) delay(1500);
  portalServer.stop();
  dns.stop();
  return configured;
}

} // namespace WifiManager
