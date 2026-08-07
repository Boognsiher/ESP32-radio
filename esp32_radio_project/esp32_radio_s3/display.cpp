#include "display.h"
#include "config.h"
#include <Arduino_GFX_Library.h>

namespace {

// Phosphor-Grün Retro-Palette (RGB565)
constexpr uint16_t COL_BG     = 0x0000;
constexpr uint16_t COL_DIM    = 0x0200;
constexpr uint16_t COL_MID    = 0x0580;
constexpr uint16_t COL_GLOW   = 0x03E0;
constexpr uint16_t COL_BRIGHT = 0x07E0;

// Eingeschriebenes Rechteck im 360px-Kreis (sicherer Textbereich, damit
// nichts an der runden Kante abgeschnitten wird).
constexpr int SAFE_L = 60, SAFE_R = 300, SAFE_T = 60;
constexpr int SAFE_W = SAFE_R - SAFE_L;
constexpr int CTR_X = TFT_WIDTH / 2, CTR_Y = TFT_HEIGHT / 2;

constexpr int Y_HEADER   = SAFE_T;
constexpr int Y_STATIONS = SAFE_T + 26;
constexpr int Y_RULE1    = SAFE_T + 44;
constexpr int Y_NAME     = SAFE_T + 70;
constexpr int Y_RULE2    = SAFE_T + 100;
constexpr int Y_ARTIST   = SAFE_T + 122;
constexpr int Y_RULE3    = SAFE_T + 160;
constexpr int Y_TITLE    = SAFE_T + 142;
constexpr int Y_STATUS   = SAFE_T + 178;
constexpr int Y_IP       = SAFE_T + 196;

constexpr unsigned long SCROLL_INTERVAL_MS = 350;
constexpr unsigned long BLINK_INTERVAL_MS  = 800;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  PIN_TFT_CS, PIN_TFT_SCK, PIN_TFT_D0, PIN_TFT_D1, PIN_TFT_D2, PIN_TFT_D3);

// Der ST77916-Konstruktor braucht zwingend die 4 Col/Row-Offset-Parameter
// (hier 0,0,0,0) UND die explizite st77916_150_init_operations-Init-
// Sequenz aus der Library selbst -- ohne beides bleibt das Bild nur
// Streifen/Bildreste (CLAUDE.md Stolperstein #1, bestätigter Aufruf aus
// reference/esp32_radio_s3_OLD.ino).
Arduino_GFX *gfx = new Arduino_ST77916(
  bus, PIN_TFT_RST, TFT_ROTATION, TFT_IPS, TFT_WIDTH, TFT_HEIGHT,
  0, 0, 0, 0,
  st77916_150_init_operations, sizeof(st77916_150_init_operations));

// --- Radio-Bildschirm-State ---
int    stationIdx = 0, stationTotal = STATION_COUNT;
String stationName;
String statusText;
bool   playing = false;
String trackArtist, trackTitle;
bool   wifiOk = false;
String ipText;
bool   onRadioScreen = false;

int scrollPos = 0;
unsigned long lastScroll = 0, lastBlink = 0;

String scrolled(const String &text, int maxChars, int &pos) {
  if ((int)text.length() <= maxChars) { pos = 0; return text; }
  String padded = text + "    ";
  int len = padded.length();
  String out;
  for (int i = 0; i < maxChars; i++) out += padded[(pos + i) % len];
  return out;
}

void drawCentered(const String &text, int y, int size, uint16_t color) {
  gfx->setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(CTR_X - w / 2, y);
  gfx->setTextColor(color, COL_BG);
  gfx->print(text);
}

void drawTitleLine() {
  gfx->fillRect(SAFE_L, Y_TITLE - 10, SAFE_W, 18, COL_BG);
  String line = trackTitle.length() ? trackTitle : (playing ? "..." : statusText);
  drawCentered(scrolled(line, 26, scrollPos), Y_TITLE, 1, COL_BRIGHT);
}

void drawStatusLine() {
  gfx->fillRect(SAFE_L, Y_STATUS - 10, SAFE_W, 36, COL_BG);
  if (playing) {
    bool blink = (millis() / BLINK_INTERVAL_MS) % 2;
    drawCentered(blink ? ">> ON AIR" : "   ON AIR", Y_STATUS, 1, COL_BRIGHT);
  } else {
    drawCentered(statusText, Y_STATUS, 1, COL_MID);
  }
  if (wifiOk) drawCentered(ipText, Y_IP, 1, COL_DIM);
}

void fullRedraw() {
  gfx->fillScreen(COL_BG);
  gfx->drawCircle(CTR_X, CTR_Y, 179, COL_DIM);

  drawCentered(">> RADIO", Y_HEADER, 2, COL_MID);

  String dots;
  for (int i = 0; i < stationTotal; i++)
    dots += (i == stationIdx) ? ("[" + String(i + 1) + "]") : (" " + String(i + 1) + " ");
  drawCentered(dots, Y_STATIONS, 1, COL_BRIGHT);

  gfx->drawFastHLine(SAFE_L, Y_RULE1, SAFE_W, COL_MID);

  String name = stationName;
  if (name.length() > 12) name = name.substring(0, 12);
  drawCentered(name, Y_NAME, 3, COL_BRIGHT);

  gfx->drawFastHLine(SAFE_L, Y_RULE2, SAFE_W, COL_MID);

  String a = trackArtist;
  if (a.length() > 26) a = a.substring(0, 26);
  drawCentered(a, Y_ARTIST, 1, COL_GLOW);

  gfx->drawFastHLine(SAFE_L, Y_RULE3, SAFE_W, COL_MID);

  drawTitleLine();
  drawStatusLine();
}

} // namespace

namespace Display {

void begin() {
  // Kein Backlight-GPIO nötig: BLK ist fest auf 3.3V verdrahtet.
  gfx->begin();
  gfx->fillScreen(COL_BG);
}

void showMessage(const String &line1, const String &line2) {
  onRadioScreen = false;
  gfx->fillScreen(COL_BG);
  gfx->drawCircle(CTR_X, CTR_Y, 179, COL_MID);
  drawCentered(line1, CTR_Y - 20, 3, COL_BRIGHT);
  if (line2.length()) drawCentered(line2, CTR_Y + 20, 1, COL_GLOW);
}

void showStation(int idx, int total, const String &name) {
  stationIdx = idx; stationTotal = total; stationName = name;
  scrollPos = 0;
  onRadioScreen = true;
  fullRedraw();
}

void setStatus(const String &text, bool isPlaying) {
  statusText = text; playing = isPlaying;
  if (onRadioScreen) drawStatusLine();
}

void setTrackInfo(const String &artist, const String &title) {
  bool changed = (trackArtist != artist) || (trackTitle != title);
  trackArtist = artist; trackTitle = title;
  scrollPos = 0;
  if (onRadioScreen && changed) fullRedraw();
}

void setWifiInfo(bool connected, const String &ip) {
  bool changed = (wifiOk != connected);
  wifiOk = connected; ipText = ip;
  if (!onRadioScreen) return;
  if (changed) fullRedraw(); else drawStatusLine();
}

void tick() {
  if (!onRadioScreen) return;
  unsigned long now = millis();
  if (now - lastScroll > SCROLL_INTERVAL_MS) {
    lastScroll = now;
    if ((int)trackTitle.length() > 26) { scrollPos++; drawTitleLine(); }
  }
  if (now - lastBlink > BLINK_INTERVAL_MS) {
    lastBlink = now;
    if (playing) drawStatusLine();
  }
}

} // namespace Display
