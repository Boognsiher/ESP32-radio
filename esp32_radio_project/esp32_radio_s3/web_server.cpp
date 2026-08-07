#include "web_server.h"
#include "config.h"
#include "i2c_protocol.h"
#include "stations.h"
#include "radio.h"
#include "i2c_master.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <stdlib.h>

namespace {
  WebServer server(80);

  // Parst "AA:BB:CC:DD:EE:FF" (gross-/kleinschreibungsunabhängig) in 6
  // Rohbytes. Gibt false bei ungültigem Format zurück.
  bool parseMacAddress(const String &text, uint8_t out[6]) {
    if (text.length() != 17) return false;
    for (int i = 0; i < 6; i++) {
      if (i < 5 && text[i * 3 + 2] != ':') return false;
      char hex[3] = { text[i * 3], text[i * 3 + 1], 0 };
      char *end = nullptr;
      long v = strtol(hex, &end, 16);
      if (end != hex + 2) return false;
      out[i] = (uint8_t)v;
    }
    return true;
  }

  // BT-Scan-Zwischenspeicher fürs Webinterface, per /btscan/data-Polling
  // aus dem Browser aktuell gehalten.
  struct WebScanDevice { String name, addr; int8_t rssi; };
  WebScanDevice webScan[I2C_SCAN_MAX_DEVICES];
  uint8_t webScanCount = 0;
  uint8_t webScanState = I2C_SCAN_STATE_IDLE;

  const char *CSS =
    "body{font-family:monospace;background:#050505;color:#00cc44;padding:16px;max-width:520px;margin:auto}"
    "h1{color:#00ff55;letter-spacing:4px;font-size:20px;border-bottom:1px solid #004422;padding-bottom:10px;margin-bottom:16px}"
    "h2{color:#00aa33;letter-spacing:2px;font-size:13px;margin:20px 0 8px}"
    ".card{border:1px solid #003311;border-radius:2px;padding:12px;margin:6px 0;background:#030d06}"
    ".card.live{border-color:#00ff55;background:#001a09}"
    ".sname{font-size:16px;font-weight:bold;color:#00ff55}"
    ".surl{font-size:10px;color:#005522;margin-top:3px;word-break:break-all}"
    ".badge{color:#00ff55;font-size:11px;float:right}"
    "label{display:block;color:#008833;font-size:11px;margin:10px 0 3px;letter-spacing:1px}"
    "input{width:100%;padding:8px;background:#030d06;border:1px solid #003311;color:#00ff55;"
    "font-family:monospace;font-size:12px;border-radius:2px;box-sizing:border-box}"
    "input:focus{outline:none;border-color:#00ff55}"
    ".btn{padding:8px 14px;border:1px solid #004422;background:#030d06;color:#00cc44;"
    "font-family:monospace;font-size:12px;cursor:pointer;border-radius:2px}"
    ".btn:hover{background:#001a09;border-color:#00ff55;color:#00ff55}"
    ".btn-full{width:100%;padding:11px;margin-top:8px;letter-spacing:2px}"
    ".status{background:#030d06;border:1px solid #003311;padding:10px;font-size:12px;line-height:2;margin-bottom:16px}"
    ".ok{color:#00ff55}.dim{color:#004422}"
    "table{width:100%;border-collapse:collapse;margin-top:12px;font-size:12px}"
    "td,th{border-bottom:1px solid #003311;padding:6px 4px;text-align:left}"
    "th{color:#008833}"
    "hr{border:none;border-top:1px solid #003311;margin:6px 0}";

  String pageHead(const String &title) {
    return "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>" + title + "</title><style>" + CSS + "</style></head><body>";
  }

  void handleRoot() {
    String html = pageHead("ESP32 RADIO");
    html += "<h1>>> ESP32 RADIO</h1>";

    html += "<div class='status'>";
    html += "SENDER&nbsp;&nbsp;: " + Stations::get(Radio::currentStation()).name + "<br>";
    if (Radio::currentArtist().length()) html += "ARTIST&nbsp;&nbsp;: " + Radio::currentArtist() + "<br>";
    if (Radio::currentTitle().length())  html += "TITEL&nbsp;&nbsp;&nbsp;: " + Radio::currentTitle() + "<br>";
    html += "STATUS&nbsp;&nbsp;: ";
    html += Radio::isPlaying() ? "<span class='ok'>[ON AIR]</span>"
                                : "<span class='dim'>[" + Radio::statusMessage() + "]</span>";
    html += "<br>NETZWERK: ";
    html += Radio::wifiConnected() ? "<span class='ok'>" + WiFi.localIP().toString() + "</span>"
                                    : "<span style='color:#cc2200'>OFFLINE</span>";
    html += "</div>";

    html += "<h2>// SENDER AUSWAHL</h2>";
    for (int i = 0; i < STATION_COUNT; i++) {
      bool live = (i == Radio::currentStation());
      Station s = Stations::get(i);
      html += "<div class='card" + String(live ? " live" : "") + "'>";
      html += "<span class='sname'>[" + String(i + 1) + "] " + s.name + "</span>";
      html += live ? "<span class='badge'>>> LIVE</span>"
                   : "<button class='btn' style='float:right' onclick=\"location='/play?i=" + String(i) + "'\">PLAY</button>";
      html += "<hr><div class='surl'>" + s.url + "</div></div>";
    }

    html += "<h2>// KONFIGURATION</h2><form method='POST' action='/save'>";
    for (int i = 0; i < STATION_COUNT; i++) {
      Station s = Stations::get(i);
      html += "<div class='card'><span class='sname'>[" + String(i + 1) + "] SENDER</span><hr>";
      html += "<label>NAME</label><input name='n" + String(i) + "' value='" + s.name + "' required maxlength='24'>";
      html += "<label>STREAM URL</label><input name='u" + String(i) + "' value='" + s.url + "' required>";
      html += "</div>";
    }
    html += "<button type='submit' class='btn btn-full'>// SPEICHERN &amp; NEUSTART</button></form>";

    html += "<h2>// SYSTEM</h2><div class='card'>";
    html += "<button class='btn' onclick=\"if(confirm('WLAN zuruecksetzen?'))location='/resetwifi'\">WLAN RESET</button> ";
    html += "<button class='btn' onclick=\"if(confirm('Neustart?'))location='/reboot'\">NEUSTART</button> ";
    html += "<button class='btn' onclick=\"location='/btscan'\">BT-GERAETE SCANNEN</button></div>";

    html += "</body></html>";
    server.send(200, "text/html", html);
  }

  void handlePlay() {
    if (server.hasArg("i")) {
      int idx = server.arg("i").toInt();
      if (idx >= 0 && idx < STATION_COUNT) Radio::startStation(idx);
    }
    server.sendHeader("Location", "/");
    server.send(302);
  }

  void handleSave() {
    for (int i = 0; i < STATION_COUNT; i++) {
      String n = server.arg("n" + String(i)); n.trim();
      String u = server.arg("u" + String(i)); u.trim();
      if (n.length() && u.length()) Stations::set(i, n, u);
    }
    Stations::save();
    Radio::startStation(Radio::currentStation());
    server.sendHeader("Location", "/");
    server.send(302);
  }

  void handleResetWifi() {
    WifiManager::clearCredentials();
    server.send(200, "text/html", pageHead("RESET") + "<h2>>> WLAN RESET - NEUSTART...</h2></body></html>");
    delay(1500);
    ESP.restart();
  }

  void handleReboot() {
    server.send(200, "text/html", pageHead("NEUSTART") + "<h2>>> NEUSTART...</h2></body></html>");
    delay(800);
    ESP.restart();
  }

  void handleBtScanPage() {
    String html = pageHead("BT-GERAETE SCAN") +
      "<h1>>> BT-GERAETE SCAN</h1>"
      "<button class='btn' onclick=\"startScan()\">SCAN STARTEN (~12s)</button>"
      "<div id='state'>Bereit.</div>"
      "<table><thead><tr><th>NAME</th><th>ADRESSE</th><th>RSSI</th><th></th></tr></thead>"
      "<tbody id='rows'></tbody></table>"
      "<h2>// FESTE MAC-ADRESSE</h2>"
      "<div class='card'>"
      "<div class='surl' style='margin-bottom:8px'>Verbindung per MAC ist zuverlaessiger als per "
      "Name (keine erneute Discovery noetig). \"VERBINDEN\" oben nutzt automatisch die MAC aus dem "
      "Scan-Ergebnis; hier kannst du alternativ eine bereits bekannte MAC direkt eintragen.</div>"
      "<label>MAC-ADRESSE (AA:BB:CC:DD:EE:FF)</label>"
      "<input id='macInput' placeholder='AA:BB:CC:DD:EE:FF' maxlength='17'>"
      "<button class='btn btn-full' onclick='connectManual()'>VERBINDEN</button>"
      "<button class='btn btn-full' style='margin-top:6px' "
      "onclick=\"if(confirm('Feste MAC entfernen und wieder per Name verbinden?'))"
      "fetch('/btscan/clearmac').then(()=>{document.getElementById('state').innerText="
      "'MAC entfernt -- verbinde wieder per Name...';});\">MAC ENTFERNEN (NAME-MODUS)</button>"
      "</div>"
      "<p><a href='/' style='color:#00aa33'>&lt;&lt; zurueck</a></p>"
      "<script>"
      "let poll=null;"
      "function startScan(){fetch('/btscan/start').then(()=>{"
      "document.getElementById('state').innerText='Scanne...';"
      "if(poll)clearInterval(poll); poll=setInterval(refresh,1500);});}"
      "function connectMac(mac,label){if(!confirm('Mit '+label+' ('+mac+') verbinden?'))return;"
      "fetch('/btscan/connect?mac='+encodeURIComponent(mac)).then(()=>{"
      "document.getElementById('state').innerText='Verbinde mit '+mac+' ...';});}"
      "function connectManual(){let m=document.getElementById('macInput').value.trim();"
      "if(!m)return; connectMac(m,m);}"
      "function refresh(){fetch('/btscan/data').then(r=>r.json()).then(d=>{"
      "let s=d.state==0?'Bereit.':(d.state==1?'Scanne...':'Fertig ('+d.count+' gefunden).');"
      "document.getElementById('state').innerText=s;"
      "let rows='';"
      "d.devices.forEach(dev=>{"
      "let btn=\"<button class='btn' onclick=\\\"connectMac('\"+dev.addr+\"','\"+dev.name.replace(/'/g,\"\\\\'\")+\"')\\\">VERBINDEN</button>\";"
      "rows+='<tr><td>'+dev.name+'</td><td>'+dev.addr+'</td><td>'+dev.rssi+'</td><td>'+btn+'</td></tr>';});"
      "document.getElementById('rows').innerHTML=rows;"
      "if(d.state==2 && poll){clearInterval(poll);poll=null;}});}"
      "refresh();"
      "</script></body></html>";
    server.send(200, "text/html", html);
  }

  void handleBtScanStart() {
    I2cMaster::triggerScan();
    webScanState = I2C_SCAN_STATE_RUNNING;
    webScanCount = 0;
    server.send(200, "text/plain", "ok");
  }

  void handleBtScanData() {
    I2cMaster::ScanStatus st;
    if (I2cMaster::getScanStatus(st)) {
      webScanState = st.state;
      webScanCount = st.count;
    }
    // Unplausible/fehlende Antwort: alten Stand beibehalten statt
    // korrupte Werte anzuzeigen (CLAUDE.md Stolperstein #6).

    if (webScanState == I2C_SCAN_STATE_DONE) {
      uint8_t valid = 0;
      for (uint8_t i = 0; i < webScanCount && i < I2C_SCAN_MAX_DEVICES; i++) {
        I2cMaster::ScanDevice dev;
        if (I2cMaster::getScanDevice(i, dev)) webScan[valid++] = { dev.name, dev.addr, dev.rssi };
      }
      webScanCount = valid;
    }

    String json = "{\"state\":" + String(webScanState) + ",\"count\":" + String(webScanCount) + ",\"devices\":[";
    for (uint8_t i = 0; i < webScanCount; i++) {
      if (i) json += ",";
      String safeName = webScan[i].name; safeName.replace("\"", "'");
      json += "{\"name\":\"" + safeName + "\",\"addr\":\"" + webScan[i].addr + "\",\"rssi\":" + String(webScan[i].rssi) + "}";
    }
    json += "]}";
    server.send(200, "application/json", json);
  }

  // MAC-Adresse wird bevorzugt (zuverlässiger, siehe README); Namens-
  // Parameter bleibt als Fallback für Clients ohne MAC-Angabe erhalten.
  void handleBtConnect() {
    if (server.hasArg("mac")) {
      String macStr = server.arg("mac"); macStr.trim();
      uint8_t mac[6];
      if (!parseMacAddress(macStr, mac)) {
        server.send(400, "text/plain", "ungueltige MAC-Adresse (Format AA:BB:CC:DD:EE:FF)");
        return;
      }
      I2cMaster::setBtTargetMac(mac);
      server.send(200, "text/plain", "ok");
      return;
    }
    if (server.hasArg("name")) {
      String name = server.arg("name"); name.trim();
      if (name.length() == 0) { server.send(400, "text/plain", "kein name"); return; }
      I2cMaster::setBtTarget(name);
      server.send(200, "text/plain", "ok");
      return;
    }
    server.send(400, "text/plain", "kein ziel angegeben (mac oder name)");
  }

  void handleBtClearMac() {
    I2cMaster::clearBtTargetMac();
    server.send(200, "text/plain", "ok");
  }
}

namespace RadioWeb {

void begin() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/play", HTTP_GET, handlePlay);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/resetwifi", HTTP_GET, handleResetWifi);
  server.on("/reboot", HTTP_GET, handleReboot);
  server.on("/btscan", HTTP_GET, handleBtScanPage);
  server.on("/btscan/start", HTTP_GET, handleBtScanStart);
  server.on("/btscan/data", HTTP_GET, handleBtScanData);
  server.on("/btscan/connect", HTTP_GET, handleBtConnect);
  server.on("/btscan/clearmac", HTTP_GET, handleBtClearMac);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302); });
  server.begin();

  if (MDNS.begin(MDNS_HOSTNAME)) MDNS.addService("http", "tcp", 80);
}

void loop() {
  server.handleClient();
}

} // namespace RadioWeb
