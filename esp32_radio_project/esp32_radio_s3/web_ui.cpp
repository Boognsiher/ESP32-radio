#include "web_ui.h"
#include "config.h"
#include "storage.h"
#include "radio_state.h"
#include "radio_audio.h"
#include "i2c_link.h"
#include <WiFi.h>
#include <WebServer.h>

static WebServer server(80);

static const char PAGE_STYLE[] =
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
  "hr{border:none;border-top:1px solid #003311;margin:6px 0}";

static void handleRoot() {
  bool playing = radioAudioIsPlaying();
  String html = String("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP32 RADIO</title><style>") + PAGE_STYLE + "</style></head><body>";

  html += "<h1>>> ESP32 RADIO</h1>";

  html += "<div class='status'>";
  html += "SENDER&nbsp;&nbsp;: " + stations[currentStation].name + "<br>";
  String artist = radioAudioCurrentArtist();
  String title  = radioAudioCurrentTitle();
  if (artist.length()) html += "ARTIST&nbsp;&nbsp;: " + artist + "<br>";
  if (title.length())  html += "TITEL&nbsp;&nbsp;&nbsp;: " + title + "<br>";
  html += "STATUS&nbsp;&nbsp;: ";
  html += playing ? "<span class='ok'>[ON AIR]</span>"
                  : "<span class='dim'>[" + radioAudioStatusMsg() + "]</span>";
  html += "<br>NETZWERK: ";
  html += wifiConnected ? "<span class='ok'>" + WiFi.localIP().toString() + "</span>"
                        : "<span style='color:#cc2200'>OFFLINE</span>";
  html += "</div>";

  html += "<h2>// SENDER AUSWAHL</h2>";
  for (int i = 0; i < STATION_COUNT; i++) {
    bool live = (i == currentStation);
    html += "<div class='card" + String(live ? " live" : "") + "'>";
    html += "<span class='sname'>[" + String(i + 1) + "] " + stations[i].name + "</span>";
    if (live) html += "<span class='badge'>>> LIVE</span>";
    else html += "<button class='btn' style='float:right' onclick=\"location='/play?i=" + String(i) + "'\">PLAY</button>";
    html += "<hr><div class='surl'>" + stations[i].url + "</div></div>";
  }

  html += "<h2>// KONFIGURATION</h2><form method='POST' action='/save'>";
  for (int i = 0; i < STATION_COUNT; i++) {
    html += "<div class='card'><span class='sname'>[" + String(i + 1) + "] SENDER</span><hr>";
    html += "<label>NAME</label><input name='n" + String(i) + "' value='" + stations[i].name + "' required maxlength='20'>";
    html += "<label>STREAM URL</label><input name='u" + String(i) + "' value='" + stations[i].url + "' required></div>";
  }
  html += "<button type='submit' class='btn btn-full'>// SPEICHERN &amp; NEUSTART</button></form>";

  html += "<h2>// SYSTEM</h2><div class='card'>";
  html += "<button class='btn' onclick=\"if(confirm('WLAN zurücksetzen?'))location='/resetwifi'\">WLAN RESET</button> ";
  html += "<button class='btn' onclick=\"if(confirm('Neustart?'))location='/reboot'\">NEUSTART</button> ";
  html += "<button class='btn' onclick=\"location='/btscan'\">BT-GERÄTE SCANNEN</button></div>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

static void handlePlay() {
  if (server.hasArg("i")) {
    int idx = server.arg("i").toInt();
    if (idx >= 0 && idx < STATION_COUNT) switchStation(idx);
  }
  server.sendHeader("Location", "/");
  server.send(302);
}

static void handleSave() {
  for (int i = 0; i < STATION_COUNT; i++) {
    String n = server.arg("n" + String(i)); n.trim();
    String u = server.arg("u" + String(i)); u.trim();
    if (n.length() > 0 && u.length() > 0) {
      stations[i].name = n;
      stations[i].url  = u;
    }
  }
  storageSaveStations(stations);
  switchStation(currentStation);
  server.sendHeader("Location", "/");
  server.send(302);
}

static void handleResetWifi() {
  storageClearWifi();
  server.send(200, "text/html",
    "<html><body style='background:#050505;color:#00cc44;font-family:monospace;padding:20px'>"
    "<h2>>> WLAN RESET - NEUSTART...</h2></body></html>");
  delay(2000);
  ESP.restart();
}

static void handleReboot() {
  server.send(200, "text/html",
    "<html><body style='background:#050505;color:#00cc44;font-family:monospace;padding:20px'>"
    "<h2>>> NEUSTART...</h2></body></html>");
  delay(1000);
  ESP.restart();
}

static void handleBtScanPage() {
  String html =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>BT-GERÄTE SCAN</title><style>"
    "body{font-family:monospace;background:#050505;color:#00cc44;padding:16px;max-width:520px;margin:auto}"
    "h1{color:#00ff55;letter-spacing:3px;font-size:18px;border-bottom:1px solid #004422;padding-bottom:10px}"
    ".btn{padding:10px 16px;border:1px solid #00cc44;background:#001a09;color:#00ff55;"
    "font-family:monospace;font-size:13px;cursor:pointer;border-radius:2px;margin:12px 0}"
    "table{width:100%;border-collapse:collapse;margin-top:12px;font-size:12px}"
    "td,th{border-bottom:1px solid #003311;padding:6px 4px;text-align:left}"
    "th{color:#008833}a{color:#00aa33}"
    ".cbtn{padding:5px 10px;border:1px solid #004422;background:#030d06;color:#00cc44;"
    "font-family:monospace;font-size:11px;cursor:pointer;border-radius:2px}"
    ".cbtn:hover{background:#001a09;border-color:#00ff55;color:#00ff55}"
    "#state{color:#00aa33;margin:8px 0;font-size:12px}"
    "</style></head><body>"
    "<h1>>> BT-GERÄTE SCAN</h1>"
    "<button class='btn' onclick=\"startScan()\">SCAN STARTEN (~12s)</button>"
    "<div id='state'>Bereit.</div>"
    "<table id='tbl'><thead><tr><th>NAME</th><th>ADRESSE</th><th>RSSI</th><th></th></tr></thead>"
    "<tbody id='rows'></tbody></table>"
    "<p><a href='/'>&lt;&lt; zurück zum Radio</a></p>"
    "<script>"
    "let poll=null;"
    "function startScan(){"
    "fetch('/btscan/start').then(()=>{"
    "document.getElementById('state').innerText='Scanne...';"
    "if(poll)clearInterval(poll);"
    "poll=setInterval(refresh,1500);"
    "});}"
    "function connectTo(name){"
    "if(!confirm('Mit \"'+name+'\" verbinden? Das BT-Board startet neu.'))return;"
    "fetch('/btscan/connect?name='+encodeURIComponent(name)).then(()=>{"
    "document.getElementById('state').innerText='Verbinde mit '+name+' ... (BT-Board startet neu)';"
    "});}"
    "function refresh(){"
    "fetch('/btscan/data').then(r=>r.json()).then(d=>{"
    "let s=d.state==0?'Bereit.':(d.state==1?'Scanne...':'Fertig ('+d.count+' gefunden).');"
    "document.getElementById('state').innerText=s;"
    "let rows='';"
    "d.devices.forEach(dev=>{"
    "let btn=dev.name=='(kein Name)'?'':\"<button class='cbtn' onclick=\\\"connectTo('\"+dev.name.replace(/'/g,\"\\\\'\")+\"')\\\">VERBINDEN</button>\";"
    "rows+='<tr><td>'+dev.name+'</td><td>'+dev.addr+'</td><td>'+dev.rssi+'</td><td>'+btn+'</td></tr>';"
    "});"
    "document.getElementById('rows').innerHTML=rows;"
    "if(d.state==2 && poll){clearInterval(poll);poll=null;}"
    "});}"
    "refresh();"
    "</script></body></html>";
  server.send(200, "text/html", html);
}

static void handleBtScanStart() {
  i2cLinkTriggerBtScan();
  server.send(200, "text/plain", "ok");
}

static void handleBtScanData() {
  i2cLinkRefreshScanResults();
  uint8_t count = i2cLinkScanCount();
  String json = "{\"state\":" + String(i2cLinkScanState()) + ",\"count\":" + String(count) + ",\"devices\":[";
  for (uint8_t i = 0; i < count; i++) {
    const ScanDeviceInfo &d = i2cLinkScanDevice(i);
    if (i > 0) json += ",";
    String safeName = d.name;
    safeName.replace("\"", "'");
    json += "{\"name\":\"" + safeName + "\",\"addr\":\"" + d.addr + "\",\"rssi\":" + String(d.rssi) + "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

static void handleBtConnect() {
  if (server.hasArg("name")) {
    String name = server.arg("name");
    name.trim();
    if (name.length() > 0 && name != "(kein Name)") {
      i2cLinkSetBtTarget(name);
      server.send(200, "text/plain", "ok");
      return;
    }
  }
  server.send(400, "text/plain", "kein gueltiger name");
}

void webUiStart() {
  server.on("/",           HTTP_GET,  handleRoot);
  server.on("/play",       HTTP_GET,  handlePlay);
  server.on("/save",       HTTP_POST, handleSave);
  server.on("/resetwifi",  HTTP_GET,  handleResetWifi);
  server.on("/reboot",     HTTP_GET,  handleReboot);
  server.on("/btscan",         HTTP_GET, handleBtScanPage);
  server.on("/btscan/start",   HTTP_GET, handleBtScanStart);
  server.on("/btscan/data",    HTTP_GET, handleBtScanData);
  server.on("/btscan/connect", HTTP_GET, handleBtConnect);
  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(302);
  });
  server.begin();
}

void webUiLoop() {
  server.handleClient();
}
