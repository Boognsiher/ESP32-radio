#include "bt_scan.h"
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>

volatile uint8_t scanDeviceCount = 0;
volatile uint8_t scanState       = 0;
ScanDevice scanDevices[SCAN_MAX_DEVICES];

static bool scanRunning = false;

static void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
      char name[249] = "";
      int  rssi = 0;
      bool hasName = false;
      for (int i = 0; i < param->disc_res.num_prop; i++) {
        esp_bt_gap_dev_prop_t *p = &param->disc_res.prop[i];
        if (p->type == ESP_BT_GAP_DEV_PROP_BDNAME) {
          int len = p->len > 248 ? 248 : p->len;
          memcpy(name, p->val, len);
          name[len] = 0;
          hasName = true;
        } else if (p->type == ESP_BT_GAP_DEV_PROP_RSSI) {
          rssi = *(int8_t *)(p->val);
        }
      }

      // Duplikate anhand der Adresse ersetzen statt neu anzuhaengen.
      int slot = -1;
      for (int i = 0; i < scanDeviceCount; i++) {
        if (memcmp(scanDevices[i].addr, param->disc_res.bda, 6) == 0) { slot = i; break; }
      }
      if (slot < 0 && scanDeviceCount < SCAN_MAX_DEVICES) {
        slot = scanDeviceCount;
        scanDeviceCount++;
      }
      if (slot >= 0) {
        memset(scanDevices[slot].name, 0, sizeof(scanDevices[slot].name));
        strncpy(scanDevices[slot].name, hasName ? name : "(kein Name)", SCAN_NAME_LEN);
        memcpy(scanDevices[slot].addr, param->disc_res.bda, 6);
        scanDevices[slot].rssi = (int8_t)rssi;
      }
      break;
    }
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
      if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
        scanRunning = false;
        scanState = 2;
      }
      break;
    default:
      break;
  }
}

void btScanInit() {
  esp_bt_gap_register_callback(gapCallback);
}

void btScanStart() {
  if (scanRunning) return;
  scanDeviceCount = 0;
  scanState = 1;
  esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, BT_SCAN_DURATION_UNITS, 0);
  scanRunning = true;
}
