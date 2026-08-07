#include "bt_scan.h"
#include "config.h"
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
  deviceCount = 0;
  scanState = I2C_SCAN_STATE_RUNNING;
  running = true;
  esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, BT_SCAN_DURATION_UNITS, 0);
  Serial.println("[SCAN] Suche nach BT-Geraeten...");
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
