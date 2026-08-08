#include "buttons.h"
#include "config.h"
#include "i2c_protocol.h"   // I2C_BTN_EVENT_SHOW_IP

namespace {
  const uint8_t PINS[STATION_COUNT] = { PIN_BTN_1, PIN_BTN_2, PIN_BTN_3 };
  unsigned long lastPress[STATION_COUNT] = { 0, 0, 0 };
  volatile uint8_t evtCounter = 0;
  volatile uint8_t station = 0;

  // Kombi Taster 1+2 (>=BUTTON_COMBO_HOLD_MS gehalten) -> IP-Anzeige-Event
  // statt Senderwechsel (I2C_BTN_EVENT_SHOW_IP).
  //
  // comboActive bleibt bewusst wahr, solange MINDESTENS EINE der beiden
  // Kombi-Tasten noch unten ist -- nicht nur, solange BEIDE unten sind.
  // Grund (Hardware-Test-Feedback): die zwei Taster werden beim Loslassen
  // praktisch nie exakt gleichzeitig frei, die zuletzt losgelassene Taste
  // wurde dadurch als frischer Einzel-Tastendruck gewertet -- das kippte
  // sofort zurück vom IP-Overlay auf den Radio-Screen UND löste einen
  // Sender-"Wechsel" (auf denselben Sender) mit vollem Stream-Reconnect
  // aus, spürbar als Hänger/Lag direkt nach der Kombi-Geste. Einzel-
  // Auswertung für 1/2 bleibt deshalb gesperrt, bis beide wieder oben sind.
  bool comboActive = false;
  bool comboFired  = false;
  unsigned long comboStart = 0;
}

namespace Buttons {

void begin() {
  for (uint8_t i = 0; i < STATION_COUNT; i++) pinMode(PINS[i], INPUT_PULLUP);
}

void poll() {
  unsigned long now = millis();
  bool btn1Down = digitalRead(PIN_BTN_1) == LOW;
  bool btn2Down = digitalRead(PIN_BTN_2) == LOW;

  if (btn1Down && btn2Down) {
    if (!comboActive) {
      comboActive = true;
      comboFired = false;
      comboStart = now;
    } else if (!comboFired && now - comboStart >= BUTTON_COMBO_HOLD_MS) {
      comboFired = true;
      station = I2C_BTN_EVENT_SHOW_IP;
      evtCounter++;
      Serial.println("[BTN] Kombi 1+2 -> IP-Adresse anzeigen");
    }
  } else if (!btn1Down && !btn2Down) {
    comboActive = false;  // beide wieder oben -- Sperre für 1/2 aufheben
  }
  // Sonst (nur noch eine der beiden unten): comboActive unverändert lassen,
  // siehe Kommentar oben -- weder hier neu starten noch aufheben.

  for (uint8_t i = 0; i < STATION_COUNT; i++) {
    if (comboActive && i < 2) continue;  // 1/2 gesperrt, solange Kombi (noch) aktiv
    if (digitalRead(PINS[i]) == LOW && now - lastPress[i] > BUTTON_DEBOUNCE_MS) {
      lastPress[i] = now;
      station = i;
      evtCounter++;
      Serial.printf("[BTN] Sender %d gewaehlt (Event %d)\n", i + 1, evtCounter);
    }
  }
}

uint8_t eventCounter() { return evtCounter; }
uint8_t currentStation() { return station; }

} // namespace Buttons
