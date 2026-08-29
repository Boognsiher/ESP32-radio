#include "web_portal.h"
#include "config.h"
#include "storage.h"
#include "display.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static const char PAGE_STYLE[] =
  "body{font-family:monospace;background:#050505;color:#00cc44;padding:20px;max-width:400px;margin:auto}"
  "h1{color:#00ff55}label{display:block;color:#008833;margin-top:12px;font-size:12px}"
  "input{width:100%;padding:10px;background:#030d06;border:1px solid #003311;color:#00ff55;"
  "font-family:monospace;border-radius:2px;box-sizing:border-box;margin-top:4px}"
  "button{width:100%;padding:12px;background:#001a09;border:1px solid #00cc44;color:#00ff55;"
  "font-family:monospace;font-weight:bold;cursor:pointer;margin-top:14px;letter-spacing:2px}";

bool webPortalRun() {
  displayMessage("WLAN SETUP", AP_SSID);

  WebServer server(80);
  DNSServer dnsServer;
  bool configured = false;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

  server.on("/", HTTP_GET, [&]() {
    String html = String("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<style>") + PAGE_STYLE + "</style></head>"
      "<body><h1>>> ESP32 RADIO</h1><h2>WLAN SETUP</h2>"
      "<form method='POST' action='/savewifi'>"
      "<label>NETZWERK (SSID)</label><input name='ssid' required>"
      "<label>PASSWORT</label><input type='password' name='pass'>"
      "<button>// VERBINDEN</button></form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/savewifi", HTTP_POST, [&]() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (ssid.length() > 0) {
      storageSaveWifi(ssid, pass);
      server.send(200, "text/html",
        "<html><body style='background:#050505;color:#00ff55;font-family:monospace;padding:20px'>"
        "<h2>>> GESPEICHERT - NEUSTART...</h2></body></html>");
      delay(2000);
      configured = true;
    }
  });

  server.onNotFound([&]() {
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302);
  });

  server.begin();
  unsigned long timeout = millis() + CAPTIVE_PORTAL_TIMEOUT_MS;
  while (!configured && millis() < timeout) {
    dnsServer.processNextRequest();
    server.handleClient();
    delay(10);
  }
  server.stop();
  dnsServer.stop();
  return configured;
}
