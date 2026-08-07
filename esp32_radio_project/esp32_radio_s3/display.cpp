#include "display.h"
#include "config.h"
#include <Arduino_GFX_Library.h>
#include <math.h>

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

// --- Wetter-Bildschirm-State ---
bool   onWeatherScreen = false;
Weather::HourSlot wxNow, wxPlus6;
Weather::DaySlot  wxToday, wxTomorrow;
bool   roomTempValid = false;
float  roomTempC = 0;

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

void drawCenteredAt(const String &text, int x, int y, int size, uint16_t color) {
  gfx->setTextSize(size);
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(x - w / 2, y);
  gfx->setTextColor(color, COL_BG);
  gfx->print(text);
}

void drawCentered(const String &text, int y, int size, uint16_t color) {
  drawCenteredAt(text, CTR_X, y, size, color);
}

// Ganzzahlig gerundete Temperatur ohne Grad-Zeichen (Standard-Font der
// GFX-Library führt u.U. kein '°'-Glyph -- "18C" statt "18°C" ist auf
// dem Gerätedisplay daher sicherer; das Webinterface nutzt echtes UTF-8).
String fmtTempC(float c) {
  return String((int)roundf(c)) + "C";
}

String fmtTempRange(float lo, float hi) {
  return String((int)roundf(lo)) + "/" + String((int)roundf(hi)) + "C";
}

// Einfache Vektor-Icons (kein Bitmap-Datenmaterial nötig), Phosphor-Grün.
void drawCloud(int cx, int cy, int r, uint16_t color) {
  gfx->fillCircle(cx - r / 3, cy, r / 3, color);
  gfx->fillCircle(cx + r / 4, cy - r / 8, r / 3, color);
  gfx->fillCircle(cx + r / 2, cy + r / 6, r / 4, color);
  gfx->fillRect(cx - r / 2, cy, r, r / 3 + 1, color);
}

void drawWeatherIcon(int cx, int cy, int r, Weather::Icon icon, uint16_t color) {
  using Icon = Weather::Icon;
  switch (icon) {
    case Icon::SUN: {
      gfx->fillCircle(cx, cy, r / 2, color);
      for (int a = 0; a < 360; a += 45) {
        float rad = a * PI / 180.0f;
        int x1 = cx + (int)(cosf(rad) * (r / 2 + 3));
        int y1 = cy + (int)(sinf(rad) * (r / 2 + 3));
        int x2 = cx + (int)(cosf(rad) * r);
        int y2 = cy + (int)(sinf(rad) * r);
        gfx->drawLine(x1, y1, x2, y2, color);
      }
      break;
    }
    case Icon::PARTLY_CLOUDY:
      gfx->fillCircle(cx - r / 3, cy - r / 4, r / 3, color);
      drawCloud(cx + r / 8, cy + r / 6, r, color);
      break;
    case Icon::CLOUDY:
      drawCloud(cx, cy, r, color);
      break;
    case Icon::FOG:
      for (int i = 0; i < 3; i++)
        gfx->drawFastHLine(cx - r, cy - r / 3 + i * (r / 3), r * 2, color);
      break;
    case Icon::RAIN:
      drawCloud(cx, cy - r / 4, r, color);
      for (int i = -1; i <= 1; i++)
        gfx->drawLine(cx + i * (r / 3), cy + r / 4, cx + i * (r / 3) - 2, cy + r / 2 + 5, color);
      break;
    case Icon::SNOW:
      drawCloud(cx, cy - r / 4, r, color);
      for (int i = -1; i <= 1; i++)
        gfx->fillCircle(cx + i * (r / 3), cy + r / 2, 2, color);
      break;
    case Icon::STORM:
      drawCloud(cx, cy - r / 4, r, color);
      gfx->fillTriangle(cx - 2, cy + r / 4, cx + 5, cy + r / 4, cx - 3, cy + r / 2 + 6, color);
      break;
    default:
      gfx->drawCircle(cx, cy, r / 2, color);
      drawCenteredAt("?", cx, cy - 4, 1, color);
      break;
  }
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

// --- Wetter-Bildschirm-Layout: 4 Spalten (jetzt/+6h/heute/morgen) ---
constexpr int WX_COL_Y_LABEL = SAFE_T + 34;
constexpr int WX_COL_Y_ICON  = SAFE_T + 68;
constexpr int WX_ICON_R      = 16;
constexpr int WX_COL_Y_TEMP  = SAFE_T + 96;
constexpr int WX_RULE_Y      = SAFE_T + 114;
constexpr int WX_ROOM_Y      = SAFE_T + 148;
constexpr int WX_ROOM_LABEL_Y = SAFE_T + 128;

int wxColX(int i) { return SAFE_L + (SAFE_W * (2 * i + 1)) / 8; }  // 4 Spalten, gleich verteilt

void drawWeatherColumn(int col, const String &label, bool valid, Weather::Icon icon, const String &tempText) {
  int x = wxColX(col);
  drawCenteredAt(label, x, WX_COL_Y_LABEL, 1, COL_MID);
  if (valid) {
    drawWeatherIcon(x, WX_COL_Y_ICON, WX_ICON_R, icon, COL_BRIGHT);
    drawCenteredAt(tempText, x, WX_COL_Y_TEMP, 1, COL_BRIGHT);
  } else {
    drawCenteredAt("n/a", x, WX_COL_Y_ICON, 1, COL_DIM);
  }
}

void weatherRedraw() {
  gfx->fillScreen(COL_BG);
  gfx->drawCircle(CTR_X, CTR_Y, 179, COL_DIM);

  drawCentered(">> WETTER", Y_HEADER, 2, COL_MID);

  drawWeatherColumn(0, "JETZT",   wxNow.valid,      wxNow.icon,      wxNow.valid ? fmtTempC(wxNow.tempC) : "");
  drawWeatherColumn(1, "+6H",     wxPlus6.valid,    wxPlus6.icon,    wxPlus6.valid ? fmtTempC(wxPlus6.tempC) : "");
  drawWeatherColumn(2, "HEUTE",   wxToday.valid,    wxToday.icon,    wxToday.valid ? fmtTempRange(wxToday.tempMin, wxToday.tempMax) : "");
  drawWeatherColumn(3, "MORGEN",  wxTomorrow.valid, wxTomorrow.icon, wxTomorrow.valid ? fmtTempRange(wxTomorrow.tempMin, wxTomorrow.tempMax) : "");

  gfx->drawFastHLine(SAFE_L, WX_RULE_Y, SAFE_W, COL_MID);

  drawCentered("RAUMTEMPERATUR", WX_ROOM_LABEL_Y, 1, COL_MID);
  drawCentered(roomTempValid ? fmtTempC(roomTempC) : "n/a", WX_ROOM_Y, 3, COL_GLOW);
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
  onWeatherScreen = false;
  gfx->fillScreen(COL_BG);
  gfx->drawCircle(CTR_X, CTR_Y, 179, COL_MID);
  drawCentered(line1, CTR_Y - 20, 3, COL_BRIGHT);
  if (line2.length()) drawCentered(line2, CTR_Y + 20, 1, COL_GLOW);
}

void showStation(int idx, int total, const String &name) {
  stationIdx = idx; stationTotal = total; stationName = name;
  scrollPos = 0;
  onRadioScreen = true;
  onWeatherScreen = false;
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

void showWeather(const Weather::HourSlot &now, const Weather::HourSlot &plus6h,
                  const Weather::DaySlot &today, const Weather::DaySlot &tomorrow,
                  bool roomValid, float roomCelsius) {
  wxNow = now; wxPlus6 = plus6h; wxToday = today; wxTomorrow = tomorrow;
  roomTempValid = roomValid; roomTempC = roomCelsius;
  onRadioScreen = false;
  onWeatherScreen = true;
  weatherRedraw();
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
