#pragma once
// Pin-Konfiguration ESP32 DevKitV1 -- siehe hardware/pinout.md und
// CLAUDE.md für die verbindliche Vorgabe. Nicht ohne expliziten
// Auftrag ändern.

// --- I2S Eingang vom Xiao S3 (Slave/RX) ---
#define PIN_I2S_BCLK      26
#define PIN_I2S_LRCK      25
#define PIN_I2S_DIN       22
#define I2S_SAMPLE_RATE   44100
#define I2S_DMA_BUF_LEN   512
#define I2S_DMA_BUF_COUNT 8

// --- I2C Slave zum Xiao S3 ---
#define PIN_I2C_SDA       32
#define PIN_I2C_SCL       33

// --- Taster (gegen GND, interner Pull-up, kein externer Widerstand nötig) ---
#define PIN_BTN_1         4
#define PIN_BTN_2         13
#define PIN_BTN_3         27
#define BUTTON_DEBOUNCE_MS 400

#define STATION_COUNT     3
#define BT_DEFAULT_NAME   "Bose"

// ESP-IDF-Inquiry-Dauer in 1.28s-Einheiten (10 -> ca. 12.8s Scan-Dauer)
#define BT_SCAN_DURATION_UNITS 10

// Retries für set_auto_reconnect() bei fester MAC-Zieladresse, bevor die
// Library auf Discovery-Scan zurückfällt (siehe bt_a2dp.cpp).
#define BT_MAC_RECONNECT_RETRIES 5
