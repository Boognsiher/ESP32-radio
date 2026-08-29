#include "display.h"
#include "config.h"
#include <Arduino_GFX_Library.h>

// Inschriebenes Rechteck im 360er Kreis (sicherer Textbereich)
#define SAFE_L   60
#define SAFE_R   300
#define SAFE_T   60
#define SAFE_W   (SAFE_R - SAFE_L)   // 240
#define CTR_X    180
#define CTR_Y    180

static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3);

// Bus- und Displaykonstruktor 1:1 aus dem bekannt funktionierenden
// Referenz-Sketch übernommen (siehe Stolperstein #1 in CLAUDE.md): die 4
// zusätzlichen Nullen sind Col/Row-Offsets, st77916_150_init_operations ist
// eine vordefinierte Init-Sequenz aus der GFX Library for Arduino selbst.
static Arduino_GFX *gfx = new Arduino_ST77916(
  bus, TFT_RST, TFT_ROTATION, TFT_IPS, TFT_WIDTH, TFT_HEIGHT,
  0, 0, 0, 0,
  st77916_150_init_operations, sizeof(st77916_150_init_operations));

static int scrollPos = 0;
static String lastScrolledTitle = "";

static String getScrolled(const String &text, int maxChars, int &pos) {
  if ((int)text.length() <= maxChars) { pos = 0; return text; }
  String padded = text + "    ";
  int len = padded.length();
  String result = "";
  for (int i = 0; i < maxChars; i++) result += padded[(pos + i) % len];
  return result;
}

static void drawCentered(const String &text, int y, int textSize, uint16_t color) {
  gfx->setTextSize(textSize);
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int x = CTR_X - (w / 2);
  gfx->setCursor(x, y);
  gfx->setTextColor(color, PHOSPHOR_BG);
  gfx->print(text);
}

void displayInit() {
  // Kein Backlight-GPIO nötig: BLK ist fest auf 3.3V verdrahtet.
  gfx->begin();
  gfx->fillScreen(PHOSPHOR_BG);
}

void displayMessage(const String &line1, const String &line2) {
  gfx->fillScreen(PHOSPHOR_BG);
  gfx->drawCircle(CTR_X, CTR_Y, 179, PHOSPHOR_MID);
  drawCentered(line1, CTR_Y - 20, 3, PHOSPHOR_BRIGHT);
  if (line2.length()) drawCentered(line2, CTR_Y + 20, 1, PHOSPHOR_GLOW);
}

void displayFullUpdate(int currentStation, const String stationNames[], bool wifiConnected) {
  gfx->fillScreen(PHOSPHOR_BG);
  gfx->drawCircle(CTR_X, CTR_Y, 179, PHOSPHOR_DIM);

  drawCentered(">> RADIO", SAFE_T, 2, PHOSPHOR_MID);

  String btns = "";
  for (int i = 0; i < STATION_COUNT; i++)
    btns += (i == currentStation) ? ("[" + String(i + 1) + "]") : (" " + String(i + 1) + " ");
  drawCentered(btns, SAFE_T + 26, 1, PHOSPHOR_BRIGHT);

  gfx->drawFastHLine(SAFE_L, SAFE_T + 44, SAFE_W, PHOSPHOR_MID);

  String name = stationNames[currentStation];
  if (name.length() > 12) name = name.substring(0, 12);
  drawCentered(name, SAFE_T + 70, 3, PHOSPHOR_BRIGHT);

  gfx->drawFastHLine(SAFE_L, SAFE_T + 100, SAFE_W, PHOSPHOR_MID);
  gfx->drawFastHLine(SAFE_L, SAFE_T + 160, SAFE_W, PHOSPHOR_MID);

  scrollPos = 0;
  lastScrolledTitle = "";
}

// Vom Aufrufer alle ~350ms aufrufen (siehe .ino loop()) – jeder Aufruf
// rückt den Scroll-Text um ein Zeichen weiter, bei Titeländerung wird
// automatisch auf Anfang zurückgesetzt.
void displayUpdateTitleLine(const String &title, bool isPlaying, const String &statusMsg) {
  gfx->fillRect(SAFE_L, SAFE_T + 132, SAFE_W, 18, PHOSPHOR_BG);
  String shown = title.length() ? title : (isPlaying ? "..." : statusMsg);
  if (shown != lastScrolledTitle) { scrollPos = 0; lastScrolledTitle = shown; }
  else if ((int)shown.length() > 26) scrollPos++;
  String displayTitle = getScrolled(shown, 26, scrollPos);
  drawCentered(displayTitle, SAFE_T + 142, 1, PHOSPHOR_BRIGHT);
}

void displayUpdateStatusLine(bool isPlaying, const String &statusMsg, bool wifiConnected, const String &ip) {
  gfx->fillRect(SAFE_L, SAFE_T + 168, SAFE_W, 36, PHOSPHOR_BG);
  if (isPlaying) {
    bool blink = (millis() / 800) % 2;
    drawCentered(blink ? ">> ON AIR" : "   ON AIR", SAFE_T + 178, 1, PHOSPHOR_BRIGHT);
  } else {
    drawCentered(statusMsg, SAFE_T + 178, 1, PHOSPHOR_MID);
  }
  if (wifiConnected) drawCentered(ip, SAFE_T + 196, 1, PHOSPHOR_DIM);
}
