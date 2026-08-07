/*
 * ESP32 DevKitV1 – Bluetooth Audio Bridge + Taster (I2C-Slave)
 * ===============================================================
 * - Empfängt Audio per I2S vom ESP32-C3 (rundes Display)
 * - Sendet per Bluetooth A2DP an Lautsprecher
 * - BT-Gerätename per Serial konfigurierbar
 * - Liest 3 Taster ein (Sender 1/2/3) und meldet die Auswahl
 *   dem C3 per I2C (als Slave), da am C3 keine freien GPIOs mehr sind
 *
 * Verdrahtung (I2S vom ESP32-C3, rundes ST77916-Display):
 *   GPIO 26 (BCLK) ← C3 GPIO 20
 *   GPIO 25 (LRCK) ← C3 GPIO 21
 *   GPIO 22 (DIN)  ← C3 GPIO 0
 *   GND            ← GND
 *
 * Verdrahtung I2C (Slave, zum C3 als Master):
 *   GPIO 32 (SDA)  ← C3 GPIO 8
 *   GPIO 33 (SCL)  ← C3 GPIO 9
 *   Externe Pull-ups (4.7kOhm) auf SDA/SCL nach 3.3V empfohlen
 *
 * Verdrahtung Taster (ein Bein → GPIO, anderes → GND):
 *   Sender 1 → GPIO 4    normaler GPIO, interner Pull-up (INPUT_PULLUP)
 *   Sender 2 → GPIO 13   normaler GPIO, interner Pull-up (INPUT_PULLUP)
 *   Sender 3 → GPIO 27   normaler GPIO, interner Pull-up (INPUT_PULLUP)
 *   -> keine externen Pull-up-Widerstände nötig
 *
 * BT-Gerätename ändern:
 *   Im Serial Monitor eingeben: setbt:Bose SoundLink Mini II
 *   Wird in NVS gespeichert und beim nächsten Start verwendet
 *
 * BT-Geräte in der Nähe auflisten:
 *   Im Serial Monitor eingeben: scan
 *   Listet ca. 12 Sekunden lang alle sichtbaren BT-Classic-Geräte mit
 *   Name, MAC-Adresse und RSSI. Danach den passenden Namen exakt so
 *   per setbt: übernehmen, wie er im Scan erscheint.
 */

#include <Arduino.h>
#include <Preferences.h>
#include <BluetoothA2DPSource.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>

// ═══════════════════════════════════════════════════════════════
// KONFIGURATION
// ═══════════════════════════════════════════════════════════════
#define I2S_BCLK        26
#define I2S_LRCK        25
#define I2S_DIN         22
#define I2S_PORT        I2S_NUM_0
#define SAMPLE_RATE     44100
#define BUFFER_SIZE     512

// --- I2C Slave (Taster-Meldung + BT-Scan-Abfrage an C3) ---
#define I2C_SDA         32
#define I2C_SCL         33
#define I2C_SLAVE_ADDR  0x42

// I2C-Kommandos (vom C3 per Wire.beginTransmission/write gesendet,
// bevor eine Antwort per Wire.requestFrom abgeholt wird)
#define I2C_CMD_BUTTONS        0x00   // Standard: 2 Byte [Event, Sender-Index]
#define I2C_CMD_START_SCAN     0x01   // startet BT-Scan, keine Antwort nötig
#define I2C_CMD_SCAN_STATUS    0x02   // Antwort: 2 Byte [state, count]
#define I2C_CMD_SCAN_DEVICE    0x10   // + Index (0..7): Antwort 27 Byte Record
#define I2C_CMD_SET_BT_NAME    0x20   // + Namensbytes: setzt neues BT-Zielgerät

#define SCAN_MAX_DEVICES  8
#define SCAN_NAME_LEN     20   // Bytes im I2C-Record (ohne Null-Terminierung)
#define BT_NAME_MAX_LEN   32

// --- Taster ---
#define BTN_1           4    // normaler GPIO, interner Pull-up
#define BTN_2           13   // normaler GPIO, interner Pull-up
#define BTN_3           27   // normaler GPIO, interner Pull-up
#define DEBOUNCE_MS     400

// ═══════════════════════════════════════════════════════════════
// GLOBALE OBJEKTE
// ═══════════════════════════════════════════════════════════════
BluetoothA2DPSource a2dp_source;
Preferences prefs;

String btDeviceName = "Bose";
bool btConnected = false;

// ═══════════════════════════════════════════════════════════════
// STATE – Taster / I2C
// ═══════════════════════════════════════════════════════════════
// volatile, da im I2C-onRequest-Callback (läuft im I2C-ISR-Kontext) gelesen
volatile uint8_t currentStationIdx = 0;   // 0..2
volatile uint8_t buttonEventCounter = 0;  // wird bei jedem Tastendruck erhöht

unsigned long lastBtn1 = 0;
unsigned long lastBtn2 = 0;
unsigned long lastBtn3 = 0;

// --- BT-Scan Ergebnisse (für I2C-Abfrage durch C3) ---
struct ScanDevice {
  char    name[SCAN_NAME_LEN + 1];
  uint8_t addr[6];
  int8_t  rssi;
};
ScanDevice scanDevices[SCAN_MAX_DEVICES];
volatile uint8_t scanDeviceCount = 0;
volatile uint8_t scanState = 0;   // 0=idle, 1=läuft, 2=fertig

volatile uint8_t i2cLastCommand      = I2C_CMD_BUTTONS;
volatile uint8_t i2cRequestedDevIdx  = 0;

// --- Ausstehender BT-Namenswechsel (aus I2C-Callback, in loop() abgearbeitet) ---
volatile bool pendingBtNameChange = false;
char pendingBtName[BT_NAME_MAX_LEN + 1] = "";

// ═══════════════════════════════════════════════════════════════
// I2S SETUP
// ═══════════════════════════════════════════════════════════════
void i2s_setup() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num   = I2S_BCLK,
    .ws_io_num    = I2S_LRCK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_DIN
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
  Serial.println("[I2S] Initialisiert");
}

// ═══════════════════════════════════════════════════════════════
// A2DP CALLBACK – liest I2S und gibt an BT weiter
// ═══════════════════════════════════════════════════════════════
int32_t bt_data_callback(Frame *frame, int32_t frame_count) {
  if (!btConnected) {
    memset(frame, 0, frame_count * sizeof(Frame));
    return frame_count;
  }

  int16_t buffer[frame_count * 2];
  size_t bytes_read = 0;

  i2s_read(I2S_PORT, buffer, frame_count * 4, &bytes_read, portMAX_DELAY);

  int frames_read = bytes_read / 4;
  for (int i = 0; i < frames_read; i++) {
    frame[i].channel1 = buffer[i * 2];
    frame[i].channel2 = buffer[i * 2 + 1];
  }
  for (int i = frames_read; i < frame_count; i++) {
    frame[i].channel1 = 0;
    frame[i].channel2 = 0;
  }
  return frame_count;
}

// ═══════════════════════════════════════════════════════════════
// BT CALLBACKS
// ═══════════════════════════════════════════════════════════════
void bt_connection_changed(esp_a2d_connection_state_t state, void *ptr) {
  btConnected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);
  if (btConnected) {
    Serial.println("[BT] ✓ Verbunden!");
  } else {
    Serial.println("[BT] Getrennt – suche neu...");
  }
}

// ═══════════════════════════════════════════════════════════════
// I2C SLAVE – Antwort auf Anfrage vom C3
// ═══════════════════════════════════════════════════════════════
// Protokoll: Der C3 schreibt optional zuerst 1 Kommando-Byte, danach
// wird per requestFrom() die passende Antwort abgeholt. Ohne
// vorheriges Schreiben (Standardfall bei jeder Sender-Abfrage) gilt
// automatisch I2C_CMD_BUTTONS, da nach jeder Antwort zurückgesetzt wird.
//   I2C_CMD_BUTTONS:      2 Byte  [Event-Zähler, Sender-Index]
//   I2C_CMD_SCAN_STATUS:  2 Byte  [scanState, deviceCount]
//   I2C_CMD_SCAN_DEVICE+i: 27 Byte [name(20) + addr(6) + rssi(1)]
//   I2C_CMD_SET_BT_NAME:   restliche Bytes = neuer BT-Gerätename (ASCII)
void onI2cReceive(int len) {
  if (Wire.available() < 1) return;
  uint8_t cmd = Wire.read();

  if (cmd == I2C_CMD_SET_BT_NAME) {
    int i = 0;
    while (Wire.available() && i < BT_NAME_MAX_LEN) {
      pendingBtName[i++] = (char)Wire.read();
    }
    pendingBtName[i] = 0;
    while (Wire.available()) Wire.read();  // Rest verwerfen falls zu lang
    pendingBtNameChange = true;
    i2cLastCommand = I2C_CMD_BUTTONS;
    return;
  }

  while (Wire.available()) Wire.read();  // evtl. Restbytes verwerfen

  if (cmd == I2C_CMD_START_SCAN) {
    startBtScan();
    i2cLastCommand = I2C_CMD_BUTTONS;
  } else if (cmd == I2C_CMD_SCAN_STATUS) {
    i2cLastCommand = I2C_CMD_SCAN_STATUS;
  } else if (cmd >= I2C_CMD_SCAN_DEVICE && cmd < I2C_CMD_SCAN_DEVICE + SCAN_MAX_DEVICES) {
    i2cLastCommand = cmd;
    i2cRequestedDevIdx = cmd - I2C_CMD_SCAN_DEVICE;
  } else {
    i2cLastCommand = I2C_CMD_BUTTONS;
  }
}

void onI2cRequest() {
  if (i2cLastCommand == I2C_CMD_SCAN_STATUS) {
    uint8_t buf[2] = { scanState, scanDeviceCount };
    Wire.write(buf, 2);
  } else if (i2cLastCommand >= I2C_CMD_SCAN_DEVICE) {
    uint8_t buf[SCAN_NAME_LEN + 6 + 1];
    memset(buf, 0, sizeof(buf));
    uint8_t idx = i2cRequestedDevIdx;
    if (idx < scanDeviceCount) {
      memcpy(buf, scanDevices[idx].name, SCAN_NAME_LEN);
      memcpy(buf + SCAN_NAME_LEN, scanDevices[idx].addr, 6);
      buf[SCAN_NAME_LEN + 6] = (uint8_t)scanDevices[idx].rssi;
    }
    Wire.write(buf, sizeof(buf));
  } else {
    uint8_t buf[2];
    buf[0] = buttonEventCounter;
    buf[1] = currentStationIdx;
    Wire.write(buf, 2);
  }
  i2cLastCommand = I2C_CMD_BUTTONS;  // zurücksetzen für nächste einfache Abfrage
}

void i2c_slave_setup() {
  Wire.begin((uint8_t)I2C_SLAVE_ADDR, I2C_SDA, I2C_SCL, 50000);
  Wire.onReceive(onI2cReceive);
  Wire.onRequest(onI2cRequest);
  Serial.println("[I2C] Slave bereit (0x42)");
}

// ═══════════════════════════════════════════════════════════════
// TASTER – Entprellung + Event-Zähler
// ═══════════════════════════════════════════════════════════════
void selectStation(uint8_t idx) {
  currentStationIdx = idx;
  buttonEventCounter++;
  Serial.printf("[BTN] Sender %d gewählt (Event %d)\n", idx + 1, buttonEventCounter);
}

void handleButtons() {
  unsigned long now = millis();

  if (digitalRead(BTN_1) == LOW && now - lastBtn1 > DEBOUNCE_MS) {
    lastBtn1 = now;
    selectStation(0);
  }
  if (digitalRead(BTN_2) == LOW && now - lastBtn2 > DEBOUNCE_MS) {
    lastBtn2 = now;
    selectStation(1);
  }
  if (digitalRead(BTN_3) == LOW && now - lastBtn3 > DEBOUNCE_MS) {
    lastBtn3 = now;
    selectStation(2);
  }
}

// ═══════════════════════════════════════════════════════════════
// BT-GERÄTE-SCAN (Diagnose) – listet sichtbare BT-Classic-Geräte
// ═══════════════════════════════════════════════════════════════
bool scanRunning = false;

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
      char name[249] = "";
      int rssi = 0;
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
      char addrStr[18];
      snprintf(addrStr, sizeof(addrStr), "%02X:%02X:%02X:%02X:%02X:%02X",
        param->disc_res.bda[0], param->disc_res.bda[1], param->disc_res.bda[2],
        param->disc_res.bda[3], param->disc_res.bda[4], param->disc_res.bda[5]);
      Serial.printf("[SCAN] %-25s  %s  RSSI=%d\n",
        hasName ? name : "(kein Name)", addrStr, rssi);

      // Im Array für I2C-Abfrage speichern (Duplikate anhand Adresse ersetzen)
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
        Serial.println("[SCAN] Fertig.");
        scanRunning = false;
        scanState = 2;
      }
      break;
    default:
      break;
  }
}

void startBtScan() {
  if (scanRunning) {
    Serial.println("[SCAN] Läuft bereits...");
    return;
  }
  Serial.println("[SCAN] Suche nach BT-Geräten (ca. 12 Sekunden)...");
  scanDeviceCount = 0;
  scanState = 1;
  esp_bt_gap_register_callback(esp_bt_gap_cb);
  esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0); // 10 * 1.28s
  scanRunning = true;
}


// ═══════════════════════════════════════════════════════════════
// NVS
// ═══════════════════════════════════════════════════════════════
void loadBtName() {
  prefs.begin("btname", true);
  btDeviceName = prefs.getString("name", "Bose");
  prefs.end();
  Serial.printf("[BT] Gerätename: %s\n", btDeviceName.c_str());
}

void saveBtName(const String &name) {
  prefs.begin("btname", false);
  prefs.putString("name", name);
  prefs.end();
}

// ═══════════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println("\n[ESP32 BT Bridge] Start");
  Serial.println("BT-Name ändern: setbt:GERÄTENAME eingeben");

  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);

  loadBtName();
  i2s_setup();
  i2c_slave_setup();

  a2dp_source.set_auto_reconnect(true);
  a2dp_source.set_on_connection_state_changed(bt_connection_changed, nullptr);
  a2dp_source.start(btDeviceName.c_str(), bt_data_callback);

  Serial.printf("[BT] Verbinde mit: %s\n", btDeviceName.c_str());
}

// ═══════════════════════════════════════════════════════════════
// LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
  handleButtons();

  // Vom Webinterface (per I2C) ausgelöster BT-Zielwechsel
  if (pendingBtNameChange) {
    pendingBtNameChange = false;
    String newName = String(pendingBtName);
    if (newName.length() > 0) {
      saveBtName(newName);
      Serial.printf("[BT] Neues Ziel per Web gesetzt: %s\n", newName.c_str());
      Serial.println("[BT] Neustart in 1 Sekunde...");
      delay(1000);
      ESP.restart();
    }
  }

  // Serial Befehle verarbeiten
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.startsWith("setbt:")) {
      String newName = cmd.substring(6);
      newName.trim();
      if (newName.length() > 0) {
        saveBtName(newName);
        Serial.printf("[BT] Neuer Name gespeichert: %s\n", newName.c_str());
        Serial.println("[BT] Neustart in 2 Sekunden...");
        delay(2000);
        ESP.restart();
      }
    } else if (cmd == "status") {
      Serial.printf("[BT] Gerät: %s | Verbunden: %s | Sender: %d\n",
        btDeviceName.c_str(), btConnected ? "JA" : "NEIN", currentStationIdx + 1);
    } else if (cmd == "scan") {
      startBtScan();
    }
  }

  delay(10);
}
