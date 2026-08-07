/*
 * Xiao ESP32-S3 – Internet Radio (rundes ST77916 QSPI-Display, 360x360)
 * ===========================================================================
 * 3 Sender, Auswahl per Taster (am DevKitV1, über I2C abgefragt) oder Webinterface
 * Phosphor-Grün Retro-Stil, angepasst an rundes Display
 *
 * PIN-SITUATION Xiao ESP32-S3 (nur 11 Header-Pins D0-D10, natives USB
 * auf separaten, nicht herausgeführten Pins):
 *   Display QSPI (ohne RST/BLK, siehe unten): 6 Pins
 *   I2S zum DevKit: 3 Pins
 *   I2C zum DevKit (Taster-Abfrage): 2 Pins
 *   -> exakt 11 von 11 Pins belegt.
 *
 * Verdrahtung Display (ST77916, QSPI, GFX Library for Arduino - moononournation):
 *   CS   -> D9  (GPIO 8)
 *   SCK  -> D8  (GPIO 7)
 *   IO0  -> D10 (GPIO 9)
 *   IO1  -> D0  (GPIO 1)
 *   IO2  -> D1  (GPIO 2)
 *   IO3  -> D2  (GPIO 3)
 *   RST  -> fest 3.3V (direkt, kein RC-Glied, kein EN-Pin-Verbund).
 *           Verlässt sich auf den internen Power-on-Reset des ST77916
 *           sowie den Software-Reset in der st77916_150_init_operations
 *           Init-Sequenz. KEIN GPIO nötig, keine Rückwirkung auf den
 *           USB-Auto-Reset-Kreis des S3.
 *   BLK  -> fest 3.3V (kein PWM-Dimmen)
 *   VCC  -> 3.3V
 *   GND  -> GND
 *
 *   I2S -> ESP32 DevKitV1 (BT-Bridge):
 *     D3  (GPIO 4)  BCLK -> DevKitV1 GPIO 26
 *     D6  (GPIO 43) LRCK -> DevKitV1 GPIO 25
 *     D7  (GPIO 44) DOUT -> DevKitV1 GPIO 22
 *     GND               -> GND (gemeinsame Masse)
 *
 *   I2C -> ESP32 DevKitV1 (Taster-Abfrage):
 *     D4 (GPIO 5, nativ SDA) -> DevKitV1 GPIO 32
 *     D5 (GPIO 6, nativ SCL) -> DevKitV1 GPIO 33
 *     Externe Pull-ups (4.7kOhm) auf SDA/SCL nach 3.3V empfohlen
 *
 * Benötigte Library:
 *   "GFX Library for Arduino" von moononournation
 *   Displaykonstruktor 1:1 aus dem funktionierenden Segeluhr-Tester-Sketch
 *   übernommen (inkl. st77916_150_init_operations Init-Sequenz), RST-Pin
 *   hier als -1 (nicht per GPIO gesteuert, siehe Verdrahtung oben).
 */

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Audio.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

// ═══════════════════════════════════════════════════════════════
// KONFIGURATION
// ═══════════════════════════════════════════════════════════════

// --- Display QSPI Pins (Xiao S3, D0-D10) ---
#define TFT_CS    8    // D9
#define TFT_SCK   7    // D8
#define TFT_D0    9    // D10 (IO0)
#define TFT_D1    1    // D0  (IO1)
#define TFT_D2    2    // D1  (IO2)
#define TFT_D3    3    // D2  (IO3)
#define TFT_RST   -1   // fest 3.3V, kein GPIO (siehe Kommentar oben)
// TFT_BLK entfällt: Backlight fest auf 3.3V verdrahtet, kein GPIO nötig

#define TFT_WIDTH   360
#define TFT_HEIGHT  360
#define TFT_ROTATION 0
#define TFT_IPS     true

// --- I2C zum DevKitV1 (Taster-Abfrage + BT-Scan) ---
#define I2C_SDA         5    // D4, nativ SDA
#define I2C_SCL         6    // D5, nativ SCL
#define I2C_SLAVE_ADDR  0x42
#define I2C_POLL_MS     80

// Müssen exakt mit den Konstanten im DevKit-Sketch übereinstimmen!
#define I2C_CMD_BUTTONS        0x00
#define I2C_CMD_START_SCAN     0x01
#define I2C_CMD_SCAN_STATUS    0x02
#define I2C_CMD_SCAN_DEVICE    0x10
#define I2C_CMD_SET_BT_NAME    0x20
#define SCAN_MAX_DEVICES       8
#define SCAN_NAME_LEN          20
#define BT_NAME_MAX_LEN        32

// --- I2S zum DevKitV1 ---
#define I2S_BCLK        4    // D3
#define I2S_LRCK        43   // D6
#define I2S_DOUT        44   // D7

#define STATIC_IP       "192.168.0.180"
#define GATEWAY         "192.168.0.254"
#define MDNS_NAME       "esp32radio"
#define AP_SSID         "ESP32-Radio"
#define AP_PASS         "12345678"

// Phosphor-Grün Farben (RGB565)
#define PHOSPHOR_BG     0x0000   // Schwarz
#define PHOSPHOR_DIM    0x0200   // Sehr dunkel grün
#define PHOSPHOR_MID    0x0580   // Mittel grün
#define PHOSPHOR_BRIGHT 0x07E0   // Helles Grün
#define PHOSPHOR_GLOW   0x03E0   // Normal Grün

// Inschriebenes Rechteck im 360er Kreis (sicherer Textbereich)
#define SAFE_L   60
#define SAFE_R   300
#define SAFE_T   60
#define SAFE_B   300
#define SAFE_W   (SAFE_R - SAFE_L)   // 240
#define CTR_X    180
#define CTR_Y    180

// ═══════════════════════════════════════════════════════════════
// DATENSTRUKTUREN
// ═══════════════════════════════════════════════════════════════
struct Station {
  String name;
  String url;
};

const Station DEFAULT_STATIONS[3] = {
  {"Swiss Jazz",     "http://stream.srg-ssr.ch/m/rsj/mp3_128"},
  {"R. Caroline",    "http://sc6.radiocaroline.net:8040/listen.pls"},
  {"Absolute 60s",   "http://ais.absoluteradio.co.uk/absolute60s.mp3"}
};

// ═══════════════════════════════════════════════════════════════
// GLOBALE OBJEKTE
// ═══════════════════════════════════════════════════════════════
Preferences prefs;
WebServer   server(80);
DNSServer   dnsServer;
Audio       audio;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3);

// Bus + Displaykonstruktor 1:1 wie im funktionierenden Segeluhr-Tester-Sketch
// übernommen: die 4 zusätzlichen Nullen sind Col/Row-Offsets, und
// st77916_150_init_operations ist eine vordefinierte Init-Sequenz, die
// direkt aus der GFX Library for Arduino kommt (kein zusätzliches Include
// nötig). Ohne diese explizite Init-Sequenz gab es Streifen/Bildreste.
Arduino_GFX *gfx = new Arduino_ST77916(
  bus, TFT_RST, TFT_ROTATION, TFT_IPS, TFT_WIDTH, TFT_HEIGHT,
  0, 0, 0, 0,
  st77916_150_init_operations, sizeof(st77916_150_init_operations));

// ═══════════════════════════════════════════════════════════════
// STATE
// ═══════════════════════════════════════════════════════════════
Station  stations[3];
int      currentStation = 0;
bool     wifiConnected  = false;
bool     isPlaying      = false;
String   currentTitle   = "";
String   currentArtist  = "";
String   statusMsg      = "Starte...";

unsigned long lastDisplayUpdate = 0;
unsigned long lastScrollTime    = 0;
unsigned long lastI2cPoll       = 0;
int           scrollPos         = 0;

uint8_t  lastButtonEvent   = 0;
bool     i2cSynced         = false;

// ═══════════════════════════════════════════════════════════════
// NVS
// ═══════════════════════════════════════════════════════════════
void saveStations() {
  prefs.begin("radio", false);
  for (int i = 0; i < 3; i++) {
    prefs.putString(("n" + String(i)).c_str(), stations[i].name);
    prefs.putString(("u" + String(i)).c_str(), stations[i].url);
  }
  prefs.end();
}

void loadStations() {
  prefs.begin("radio", true);
  bool hasData = prefs.isKey("n0");
  prefs.end();
  if (!hasData) {
    for (int i = 0; i < 3; i++) stations[i] = {DEFAULT_STATIONS[i].name, DEFAULT_STATIONS[i].url};
    saveStations();
    return;
  }
  prefs.begin("radio", true);
  for (int i = 0; i < 3; i++) {
    stations[i].name = prefs.getString(("n" + String(i)).c_str(), DEFAULT_STATIONS[i].name);
    stations[i].url  = prefs.getString(("u" + String(i)).c_str(), DEFAULT_STATIONS[i].url);
  }
  prefs.end();
}

int loadCurrentStation() {
  prefs.begin("radio", true);
  int idx = prefs.getInt("cur", 0);
  prefs.end();
  return constrain(idx, 0, 2);
}

void saveCurrentStation(int idx) {
  prefs.begin("radio", false);
  prefs.putInt("cur", idx);
  prefs.end();
}

bool loadWifi(String &ssid, String &pass) {
  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();
  return ssid.length() > 0;
}

void saveWifi(const String &ssid, const String &pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

// ═══════════════════════════════════════════════════════════════
// DISPLAY – Phosphor-Grün Retro (rund, 360x360)
// ═══════════════════════════════════════════════════════════════
void displayInit() {
  // Kein Backlight-GPIO nötig: BLK ist fest auf 3.3V verdrahtet
  gfx->begin();
  gfx->fillScreen(PHOSPHOR_BG);
}

String getScrolled(const String &text, int maxChars, int &pos) {
  if ((int)text.length() <= maxChars) { pos = 0; return text; }
  String padded = text + "    ";
  int len = padded.length();
  String result = "";
  for (int i = 0; i < maxChars; i++)
    result += padded[(pos + i) % len];
  return result;
}

void drawCentered(const String &text, int y, int textSize, uint16_t color) {
  gfx->setTextSize(textSize);
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int x = CTR_X - (w / 2);
  gfx->setCursor(x, y);
  gfx->setTextColor(color, PHOSPHOR_BG);
  gfx->print(text);
}

void displayUpdate() {
  gfx->fillScreen(PHOSPHOR_BG);
  gfx->drawCircle(CTR_X, CTR_Y, 179, PHOSPHOR_DIM);

  drawCentered(">> RADIO", SAFE_T, 2, PHOSPHOR_MID);

  String btns = "";
  for (int i = 0; i < 3; i++) {
    btns += (i == currentStation) ? ("[" + String(i + 1) + "]") : (" " + String(i + 1) + " ");
  }
  drawCentered(btns, SAFE_T + 26, 1, PHOSPHOR_BRIGHT);

  gfx->drawFastHLine(SAFE_L, SAFE_T + 44, SAFE_W, PHOSPHOR_MID);

  String name = stations[currentStation].name;
  if (name.length() > 12) name = name.substring(0, 12);
  drawCentered(name, SAFE_T + 70, 3, PHOSPHOR_BRIGHT);

  gfx->drawFastHLine(SAFE_L, SAFE_T + 100, SAFE_W, PHOSPHOR_MID);

  String artist = currentArtist;
  if (artist.length() > 26) artist = artist.substring(0, 26);
  drawCentered(artist, SAFE_T + 122, 1, PHOSPHOR_GLOW);

  gfx->drawFastHLine(SAFE_L, SAFE_T + 160, SAFE_W, PHOSPHOR_MID);

  // Dynamische Zeilen (Titel-Scroll, Status/Blink, IP) werden von den
  // leichten Funktionen unten gezeichnet, hier nur initial mit aufrufen:
  drawTitleLine();
  drawStatusLine();
}

// Zeichnet NUR die Titel-Zeile neu (für Scroll-Text) – kein Vollbild-Clear,
// verhindert Flackern bei häufigen Aktualisierungen.
void drawTitleLine() {
  gfx->fillRect(SAFE_L, SAFE_T + 132, SAFE_W, 18, PHOSPHOR_BG);
  String title = currentTitle.length() ? currentTitle : (isPlaying ? "..." : statusMsg);
  String displayTitle = getScrolled(title, 26, scrollPos);
  drawCentered(displayTitle, SAFE_T + 142, 1, PHOSPHOR_BRIGHT);
}

// Zeichnet NUR Status-Zeile + IP neu (für ON-AIR-Blinken) – kein Vollbild-Clear.
void drawStatusLine() {
  gfx->fillRect(SAFE_L, SAFE_T + 168, SAFE_W, 36, PHOSPHOR_BG);
  if (isPlaying) {
    bool blink = (millis() / 800) % 2;
    drawCentered(blink ? ">> ON AIR" : "   ON AIR", SAFE_T + 178, 1, PHOSPHOR_BRIGHT);
  } else {
    drawCentered(statusMsg, SAFE_T + 178, 1, PHOSPHOR_MID);
  }

  if (wifiConnected) {
    drawCentered(WiFi.localIP().toString(), SAFE_T + 196, 1, PHOSPHOR_DIM);
  }
}

void displayMessage(String line1, String line2 = "") {
  gfx->fillScreen(PHOSPHOR_BG);
  gfx->drawCircle(CTR_X, CTR_Y, 179, PHOSPHOR_MID);
  drawCentered(line1, CTR_Y - 20, 3, PHOSPHOR_BRIGHT);
  if (line2.length()) {
    drawCentered(line2, CTR_Y + 20, 1, PHOSPHOR_GLOW);
  }
}

// ═══════════════════════════════════════════════════════════════
// I2C MASTER – Taster-Abfrage am DevKitV1
// ═══════════════════════════════════════════════════════════════
// Protokoll: DevKit liefert auf Anfrage 2 Bytes:
//   Byte 0 = Event-Zähler (wird bei jedem Tastendruck am DevKit erhöht)
//   Byte 1 = zuletzt gewählter Sender-Index (0..2)
// Der C3 merkt sich den letzten gesehenen Event-Zähler und wechselt den
// Sender nur, wenn sich der Zähler geändert hat. So bleibt die Web-Auswahl
// unabhängig von einem "alten" Tasterzustand am DevKit.
void pollButtons() {
  uint8_t received = Wire.requestFrom(I2C_SLAVE_ADDR, 2);
  if (received < 2 || Wire.available() < 2) {
    Serial.printf("[I2C] Keine/zu wenig Antwort vom DevKit (received=%d)\n", received);
    return;
  }

  uint8_t evt = Wire.read();
  uint8_t idx = Wire.read();
  Serial.printf("[I2C] evt=%d idx=%d (lastEvt=%d, synced=%d)\n", evt, idx, lastButtonEvent, i2cSynced);

  if (!i2cSynced) {
    lastButtonEvent = evt;
    i2cSynced = true;
    return;
  }

  if (evt != lastButtonEvent) {
    lastButtonEvent = evt;
    if (idx <= 2) startStation(idx);
  }
}

// ═══════════════════════════════════════════════════════════════
// I2C MASTER – BT-Scan steuern/abfragen (für Webinterface)
// ═══════════════════════════════════════════════════════════════
struct WebScanDevice {
  String  name;
  String  addr;
  int8_t  rssi;
};
WebScanDevice webScanResults[SCAN_MAX_DEVICES];
uint8_t webScanCount = 0;
uint8_t webScanState = 0;  // 0=idle, 1=läuft, 2=fertig

void i2cSendCommand(uint8_t cmd) {
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(cmd);
  Wire.endTransmission();
}

void i2cTriggerBtScan() {
  i2cSendCommand(I2C_CMD_START_SCAN);
  webScanState = 1;
  webScanCount = 0;
}

// Sendet Kommando + Namensbytes in einer Transaktion, DevKit speichert
// den Namen und startet neu, um sich mit dem neuen Ziel zu verbinden.
void i2cSetBtTarget(const String &name) {
  String trimmed = name;
  if (trimmed.length() > BT_NAME_MAX_LEN) trimmed = trimmed.substring(0, BT_NAME_MAX_LEN);
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(I2C_CMD_SET_BT_NAME);
  Wire.write((const uint8_t *)trimmed.c_str(), trimmed.length());
  Wire.endTransmission();
}

// Holt Status + (falls fertig) alle Geräte-Datensätze vom DevKit und
// füllt webScanResults/webScanCount/webScanState.
// Enthält einfache Plausibilitätsprüfungen, um offensichtlich korrupte
// I2C-Antworten (z.B. durch fehlende Pull-ups) nicht als Ergebnis
// anzuzeigen, statt ihnen blind zu vertrauen.
void i2cRefreshScanResults() {
  i2cSendCommand(I2C_CMD_SCAN_STATUS);
  delay(5);
  Wire.requestFrom(I2C_SLAVE_ADDR, 2);
  if (Wire.available() < 2) return;
  uint8_t newState = Wire.read();
  uint8_t count = Wire.read();

  // Plausibilität: state muss 0/1/2 sein, count darf Maximum nicht
  // übersteigen. Bei Unplausibilität: alte Werte behalten, nicht
  // überschreiben (vermutlich korrupter I2C-Read).
  if (newState > 2 || count > SCAN_MAX_DEVICES) {
    Serial.printf("[I2C] Unplausible Scan-Status-Antwort verworfen (state=%d, count=%d)\n", newState, count);
    return;
  }
  webScanState = newState;
  webScanCount = count;

  if (webScanState != 2) return;  // nur bei "fertig" Geräte abholen

  uint8_t validCount = 0;
  for (uint8_t i = 0; i < webScanCount; i++) {
    i2cSendCommand(I2C_CMD_SCAN_DEVICE + i);
    delay(5);
    uint8_t need = SCAN_NAME_LEN + 6 + 1;
    Wire.requestFrom(I2C_SLAVE_ADDR, need);
    if (Wire.available() < need) continue;

    char nameBuf[SCAN_NAME_LEN + 1];
    for (int b = 0; b < SCAN_NAME_LEN; b++) nameBuf[b] = Wire.read();
    nameBuf[SCAN_NAME_LEN] = 0;

    uint8_t addr[6];
    for (int b = 0; b < 6; b++) addr[b] = Wire.read();
    int8_t rssi = (int8_t)Wire.read();

    // Leere/Nullwerte deuten auf einen nicht befüllten Slot oder eine
    // korrupte Übertragung hin -> Eintrag verwerfen statt leer anzeigen.
    bool addrAllZero = true;
    for (int b = 0; b < 6; b++) if (addr[b] != 0) { addrAllZero = false; break; }
    if (addrAllZero && nameBuf[0] == 0) continue;

    char addrStr[18];
    snprintf(addrStr, sizeof(addrStr), "%02X:%02X:%02X:%02X:%02X:%02X",
      addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    webScanResults[validCount].name = String(nameBuf);
    webScanResults[validCount].addr = String(addrStr);
    webScanResults[validCount].rssi = rssi;
    validCount++;
  }
  webScanCount = validCount;
}

// ═══════════════════════════════════════════════════════════════
// AUDIO CALLBACKS
// ═══════════════════════════════════════════════════════════════
void audio_showstreamtitle(const char *info) {
  String s = String(info);
  s.trim();
  int sep = s.indexOf(" - ");
  if (sep > 0) {
    currentArtist = s.substring(0, sep);
    currentTitle  = s.substring(sep + 3);
  } else {
    currentTitle  = s;
    currentArtist = "";
  }
  scrollPos = 0;
}

void audio_eof_mp3(const char *info) {
  Serial.printf("[EOF] %s\n", info);
  statusMsg = "Reconnect...";
  isPlaying = false;
  delay(2000);
  if (wifiConnected) {
    audio.connecttohost(stations[currentStation].url.c_str());
    statusMsg = "Spielt";
    isPlaying = true;
  }
}

// ═══════════════════════════════════════════════════════════════
// RADIO
// ═══════════════════════════════════════════════════════════════
void startStation(int idx) {
  currentStation = ((idx % 3) + 3) % 3;
  currentTitle   = "";
  currentArtist  = "";
  scrollPos      = 0;
  statusMsg      = "Verbinde...";
  isPlaying      = false;
  audio.stopSong();
  delay(200);
  if (wifiConnected) {
    audio.connecttohost(stations[currentStation].url.c_str());
    statusMsg = "Spielt";
    isPlaying = true;
  }
  saveCurrentStation(currentStation);
  displayUpdate();
}

// ═══════════════════════════════════════════════════════════════
// WEBSERVER – Retro Monospace Style
// ═══════════════════════════════════════════════════════════════
void handleRoot() {
  String html =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP32 RADIO</title><style>"
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
    "hr{border:none;border-top:1px solid #003311;margin:6px 0}"
    "</style></head><body>";

  html += "<h1>>> ESP32 RADIO</h1>";

  html += "<div class='status'>";
  html += "SENDER&nbsp;&nbsp;: " + stations[currentStation].name + "<br>";
  if (currentArtist.length()) html += "ARTIST&nbsp;&nbsp;: " + currentArtist + "<br>";
  if (currentTitle.length())  html += "TITEL&nbsp;&nbsp;&nbsp;: " + currentTitle  + "<br>";
  html += "STATUS&nbsp;&nbsp;: ";
  html += isPlaying ? "<span class='ok'>[ON AIR]</span>" : "<span class='dim'>[" + statusMsg + "]</span>";
  html += "<br>";
  html += "NETZWERK: ";
  html += wifiConnected ? "<span class='ok'>" + WiFi.localIP().toString() + "</span>"
                        : "<span style='color:#cc2200'>OFFLINE</span>";
  html += "</div>";

  html += "<h2>// SENDER AUSWAHL</h2>";
  for (int i = 0; i < 3; i++) {
    bool live = (i == currentStation);
    html += "<div class='card" + String(live ? " live" : "") + "'>";
    html += "<span class='sname'>[" + String(i+1) + "] " + stations[i].name + "</span>";
    if (live) html += "<span class='badge'>>> LIVE</span>";
    else html += "<button class='btn' style='float:right' onclick=\"location='/play?i=" + String(i) + "'\">PLAY</button>";
    html += "<hr><div class='surl'>" + stations[i].url + "</div>";
    html += "</div>";
  }

  html += "<h2>// KONFIGURATION</h2>";
  html += "<form method='POST' action='/save'>";
  for (int i = 0; i < 3; i++) {
    html += "<div class='card'>";
    html += "<span class='sname'>[" + String(i+1) + "] SENDER</span><hr>";
    html += "<label>NAME</label><input name='n" + String(i) + "' value='" + stations[i].name + "' required maxlength='20'>";
    html += "<label>STREAM URL</label><input name='u" + String(i) + "' value='" + stations[i].url + "' required>";
    html += "</div>";
  }
  html += "<button type='submit' class='btn btn-full'>// SPEICHERN &amp; NEUSTART</button>";
  html += "</form>";

  html += "<h2>// SYSTEM</h2>";
  html += "<div class='card'>";
  html += "<button class='btn' onclick=\"if(confirm('WLAN zurücksetzen?'))location='/resetwifi'\">WLAN RESET</button> ";
  html += "<button class='btn' onclick=\"if(confirm('Neustart?'))location='/reboot'\">NEUSTART</button> ";
  html += "<button class='btn' onclick=\"location='/btscan'\">BT-GERÄTE SCANNEN</button>";
  html += "</div>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handlePlay() {
  if (server.hasArg("i")) {
    int idx = server.arg("i").toInt();
    if (idx >= 0 && idx <= 2) startStation(idx);
  }
  server.sendHeader("Location", "/"); server.send(302);
}

void handleSave() {
  for (int i = 0; i < 3; i++) {
    String n = server.arg("n" + String(i)); n.trim();
    String u = server.arg("u" + String(i)); u.trim();
    if (n.length() > 0 && u.length() > 0) {
      stations[i].name = n;
      stations[i].url  = u;
    }
  }
  saveStations();
  startStation(currentStation);
  server.sendHeader("Location", "/"); server.send(302);
}

void handleResetWifi() {
  prefs.begin("wifi", false); prefs.clear(); prefs.end();
  server.send(200, "text/html",
    "<html><body style='background:#050505;color:#00cc44;font-family:monospace;padding:20px'>"
    "<h2>>> WLAN RESET – NEUSTART...</h2></body></html>");
  delay(2000); ESP.restart();
}

void handleReboot() {
  server.send(200, "text/html",
    "<html><body style='background:#050505;color:#00cc44;font-family:monospace;padding:20px'>"
    "<h2>>> NEUSTART...</h2></body></html>");
  delay(1000); ESP.restart();
}

void handleBtScanPage() {
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
    "th{color:#008833}"
    "a{color:#00aa33}"
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

void handleBtScanStart() {
  i2cTriggerBtScan();
  server.send(200, "text/plain", "ok");
}

void handleBtScanData() {
  i2cRefreshScanResults();
  String json = "{\"state\":" + String(webScanState) + ",\"count\":" + String(webScanCount) + ",\"devices\":[";
  for (uint8_t i = 0; i < webScanCount; i++) {
    if (i > 0) json += ",";
    String safeName = webScanResults[i].name;
    safeName.replace("\"", "'");
    json += "{\"name\":\"" + safeName + "\",\"addr\":\"" + webScanResults[i].addr +
            "\",\"rssi\":" + String(webScanResults[i].rssi) + "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleBtConnect() {
  if (server.hasArg("name")) {
    String name = server.arg("name");
    name.trim();
    if (name.length() > 0 && name != "(kein Name)") {
      i2cSetBtTarget(name);
      server.send(200, "text/plain", "ok");
      return;
    }
  }
  server.send(400, "text/plain", "kein gueltiger name");
}

void startWebserver() {
  server.on("/",          HTTP_GET,  handleRoot);
  server.on("/play",      HTTP_GET,  handlePlay);
  server.on("/save",      HTTP_POST, handleSave);
  server.on("/resetwifi", HTTP_GET,  handleResetWifi);
  server.on("/reboot",    HTTP_GET,  handleReboot);
  server.on("/btscan",        HTTP_GET, handleBtScanPage);
  server.on("/btscan/start",  HTTP_GET, handleBtScanStart);
  server.on("/btscan/data",   HTTP_GET, handleBtScanData);
  server.on("/btscan/connect",HTTP_GET, handleBtConnect);
  server.onNotFound([]() {
    server.sendHeader("Location", "/"); server.send(302);
  });
  server.begin();
}

// ═══════════════════════════════════════════════════════════════
// CAPTIVE PORTAL
// ═══════════════════════════════════════════════════════════════
bool startCaptivePortal() {
  displayMessage("WLAN SETUP", AP_SSID);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  dnsServer.start(53, "*", IPAddress(192,168,4,1));
  bool configured = false;

  server.on("/", HTTP_GET, [&]() {
    server.send(200, "text/html",
      "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<style>body{font-family:monospace;background:#050505;color:#00cc44;padding:20px;max-width:400px;margin:auto}"
      "h1{color:#00ff55}label{display:block;color:#008833;margin-top:12px;font-size:12px}"
      "input{width:100%;padding:10px;background:#030d06;border:1px solid #003311;color:#00ff55;"
      "font-family:monospace;border-radius:2px;box-sizing:border-box;margin-top:4px}"
      "button{width:100%;padding:12px;background:#001a09;border:1px solid #00cc44;color:#00ff55;"
      "font-family:monospace;font-weight:bold;cursor:pointer;margin-top:14px;letter-spacing:2px}</style></head>"
      "<body><h1>>> ESP32 RADIO</h1><h2>WLAN SETUP</h2>"
      "<form method='POST' action='/savewifi'>"
      "<label>NETZWERK (SSID)</label><input name='ssid' required>"
      "<label>PASSWORT</label><input type='password' name='pass'>"
      "<button>// VERBINDEN</button></form></body></html>");
  });

  server.on("/savewifi", HTTP_POST, [&]() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (ssid.length() > 0) {
      saveWifi(ssid, pass);
      server.send(200, "text/html",
        "<html><body style='background:#050505;color:#00ff55;font-family:monospace;padding:20px'>"
        "<h2>>> GESPEICHERT – NEUSTART...</h2></body></html>");
      delay(2000);
      configured = true;
    }
  });

  server.onNotFound([&]() {
    server.sendHeader("Location", "http://192.168.4.1/"); server.send(302);
  });

  server.begin();
  unsigned long timeout = millis() + 180000;
  while (!configured && millis() < timeout) {
    dnsServer.processNextRequest();
    server.handleClient();
    delay(10);
  }
  server.stop();
  dnsServer.stop();
  return configured;
}

// ═══════════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(I2C_SDA, I2C_SCL, 50000);

  displayInit();

  displayMessage("RADIO", "S3 Round  v5.0");
  delay(1500);

  loadStations();
  currentStation = loadCurrentStation();

  String ssid, pass;
  if (!loadWifi(ssid, pass)) {
    bool ok = startCaptivePortal();
    if (ok) ESP.restart();
    displayMessage("KEIN WLAN", "NEUSTART NÖTIG");
    while (true) delay(1000);
  }

  displayMessage("WLAN...", ssid);

  IPAddress ip, gw, sn, dns1;
  ip.fromString(STATIC_IP);
  gw.fromString(GATEWAY);
  sn.fromString("255.255.255.0");
  dns1.fromString("8.8.8.8");
  WiFi.config(ip, gw, sn, dns1);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  for (int i = 0; WiFi.status() != WL_CONNECTED && i < 30; i++) {
    delay(500);
    String dots = "";
    for (int d = 0; d < (i % 4); d++) dots += ".";
    displayMessage("VERBINDE" + dots, ssid);
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("[WiFi] %s\n", WiFi.localIP().toString().c_str());
    if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);
    startWebserver();
    audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);
    audio.setVolume(17);
    startStation(currentStation);
  } else {
    // Verbindung fehlgeschlagen (z.B. falsches Passwort) -> automatisch
    // den Hotspot/Captive Portal zur Neueingabe öffnen, kein Reflash nötig.
    wifiConnected = false;
    displayMessage("WLAN FEHLER", "NEUE EINGABE...");
    delay(1500);
    bool ok = startCaptivePortal();
    if (ok) ESP.restart();
    // Portal-Timeout ohne neue Eingabe: einfach neu starten und mit den
    // (evtl. weiterhin falschen) gespeicherten Daten erneut versuchen.
    ESP.restart();
  }

  displayUpdate();
}

// ═══════════════════════════════════════════════════════════════
// LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
  server.handleClient();
  audio.loop();

  unsigned long now = millis();

  if (now - lastScrollTime > 350) {
    lastScrollTime = now;
    if (currentTitle.length() > 26) {
      scrollPos++;
      drawTitleLine();
    }
  }

  // Blinkendes ON AIR alle 800ms, IP-Anzeige aktuell halten – nur diese
  // Zeile neu zeichnen, kein Vollbild-Refresh (verhindert Flackern)
  if (now - lastDisplayUpdate > 800) {
    lastDisplayUpdate = now;
    drawStatusLine();
  }

  if (now - lastI2cPoll > I2C_POLL_MS) {
    lastI2cPoll = now;
    pollButtons();
  }

  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    isPlaying     = false;
    statusMsg     = "WLAN VERLOREN";
  }

  if (!wifiConnected && WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    delay(1000);
    startStation(currentStation);
  }

  delay(5);
}
