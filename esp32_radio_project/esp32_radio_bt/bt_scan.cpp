#include "bt_scan.h"
#include "config.h"
#include "bt_a2dp.h"
#include <esp_bt.h>
#include <esp_gap_bt_api.h>
#include <string.h>
#include <stdio.h>

namespace {
  struct Device {
    char    name[I2C_SCAN_NAME_LEN + 1];
    uint8_t addr[6];
    int8_t  rssi;
  };
  Device  devices[I2C_SCAN_MAX_DEVICES];
  volatile uint8_t deviceCount = 0;
  volatile uint8_t scanState   = I2C_SCAN_STATE_IDLE;
  bool running = false;
  unsigned long scanStartedAt = 0;

  // Sicherheitsnetz: BT_SCAN_DURATION_UNITS*1.28s ist die normale Dauer,
  // bis ESP_BT_GAP_DISCOVERY_STOPPED feuert. Falls die Inquiry aus
  // irgendeinem Grund nie sauber stoppt, bleibt "running" sonst für immer
  // hängen und jeder weitere start()-Aufruf wird stillschweigend ignoriert
  // (Hardware-Test-Feedback: Scan blieb dauerhaft in "läuft"). loop()
  // erzwingt nach dieser Zeit einen Reset.
  constexpr unsigned long SCAN_TIMEOUT_MS = (BT_SCAN_DURATION_UNITS * 1280UL) + 5000UL;

  void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
      case ESP_BT_GAP_DISC_RES_EVT: {
        char name[249] = "";
        int8_t rssi = 0;
        bool hasName = false;
        for (int i = 0; i < param->disc_res.num_prop; i++) {
          esp_bt_gap_dev_prop_t *p = &param->disc_res.prop[i];
          if (p->type == ESP_BT_GAP_DEV_PROP_BDNAME) {
            int len = p->len > 248 ? 248 : p->len;
            memcpy(name, p->val, len);
            name[len] = 0;
            hasName = true;
          } else if (p->type == ESP_BT_GAP_DEV_PROP_RSSI) {
            rssi = *(int8_t *)p->val;
          }
        }

        // Duplikate anhand der Adresse ersetzen statt anhängen.
        int slot = -1;
        for (int i = 0; i < deviceCount; i++) {
          if (memcmp(devices[i].addr, param->disc_res.bda, 6) == 0) { slot = i; break; }
        }
        if (slot < 0 && deviceCount < I2C_SCAN_MAX_DEVICES) { slot = deviceCount; deviceCount++; }
        if (slot >= 0) {
          memset(devices[slot].name, 0, sizeof(devices[slot].name));
          strncpy(devices[slot].name, hasName ? name : "(kein Name)", I2C_SCAN_NAME_LEN);
          memcpy(devices[slot].addr, param->disc_res.bda, 6);
          devices[slot].rssi = rssi;
        }

        char addrStr[18];
        snprintf(addrStr, sizeof(addrStr), "%02X:%02X:%02X:%02X:%02X:%02X",
          param->disc_res.bda[0], param->disc_res.bda[1], param->disc_res.bda[2],
          param->disc_res.bda[3], param->disc_res.bda[4], param->disc_res.bda[5]);
        Serial.printf("[SCAN] %-25s %s RSSI=%d\n", hasName ? name : "(kein Name)", addrStr, rssi);
        break;
      }
      case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
          scanState = I2C_SCAN_STATE_DONE;
          running = false;
          Serial.println("[SCAN] Fertig.");
        }
        break;
      default:
        break;
    }
  }
}

namespace BtScan {

void begin() {
  esp_bt_gap_register_callback(gapCallback);
}

void start() {
  if (running) { Serial.println("[SCAN] Laeuft bereits."); return; }

  // WICHTIG (Hardware-Test-Feedback): Ein automatisches Trennen der
  // laufenden A2DP-Verbindung unmittelbar vor der Inquiry wurde
  // ausprobiert (BtA2dp::disconnect() gefolgt von
  // esp_bt_gap_start_discovery()) -- selbst mit Warten auf die
  // Trennungsbestätigung brachte das den kompletten Classic-BT-Stack
  // zuverlässig zum Hängen (Board reagierte auf gar nichts mehr, auch
  // nicht auf "status" über Serial -- nur ein esptool-Hard-Reset half).
  // Deshalb bewusst NICHT automatisch trennen, sondern den Scan
  // ablehnen, solange eine Verbindung besteht. Um zu scannen: Board neu
  // starten (kurzes Zeitfenster vor dem Auto-Reconnect) oder
  // "clearbtmac"/"setbt:" mit einem Platzhalter-Namen setzen, der
  // absichtlich nicht verbindet.
  if (BtA2dp::isConnected()) {
    Serial.println("[SCAN] Abgelehnt: erst trennen (verbunden mit " + BtA2dp::deviceName() + "), Scan+aktive A2DP-Verbindung hängt die BT-Radio auf.");
    return;
  }

  deviceCount = 0;
  scanState = I2C_SCAN_STATE_RUNNING;
  running = true;
  scanStartedAt = millis();
  esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, BT_SCAN_DURATION_UNITS, 0);
  Serial.println("[SCAN] Suche nach BT-Geraeten...");
}

void loop() {
  if (running && millis() - scanStartedAt > SCAN_TIMEOUT_MS) {
    Serial.println("[SCAN] Timeout -- erzwinge Abbruch (kein DISCOVERY_STOPPED-Event erhalten).");
    esp_bt_gap_cancel_discovery();
    scanState = I2C_SCAN_STATE_DONE;
    running = false;
  }
}

uint8_t state() { return scanState; }
uint8_t count() { return deviceCount; }

bool getDevice(uint8_t index, char nameOut[I2C_SCAN_NAME_LEN], uint8_t addrOut[6], int8_t &rssiOut) {
  if (index >= deviceCount) return false;
  memset(nameOut, 0, I2C_SCAN_NAME_LEN);
  strncpy(nameOut, devices[index].name, I2C_SCAN_NAME_LEN);
  memcpy(addrOut, devices[index].addr, 6);
  rssiOut = devices[index].rssi;
  return true;
}

} // namespace BtScan
