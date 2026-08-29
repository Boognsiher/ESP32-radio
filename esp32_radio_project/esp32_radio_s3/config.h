// Zentrale Konfiguration – Xiao ESP32-S3
// Pin-Zuordnung siehe hardware/pinout.md und CLAUDE.md.
#pragma once

// --- Display QSPI (ST77916, rund, 360x360) ---
#define TFT_CS    8    // D9
#define TFT_SCK   7    // D8
#define TFT_D0    9    // D10 (IO0)
#define TFT_D1    1    // D0  (IO1)
#define TFT_D2    2    // D1  (IO2)
#define TFT_D3    3    // D2  (IO3)
#define TFT_RST   -1   // fest 3.3V, kein GPIO (siehe Stolperstein #1)
#define TFT_WIDTH   360
#define TFT_HEIGHT  360
#define TFT_ROTATION 0
#define TFT_IPS     true

// --- I2S zum DevKitV1 (S3 ist Master/Taktquelle) ---
#define I2S_BCLK        4    // D3
#define I2S_LRCK        43   // D6
#define I2S_DOUT        44   // D7
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS    2
#define AUDIO_BITS        16

// Stolperstein #12: Serial-Debug im Audio-Hot-Path stört hörbar den Stream.
// Nur für gezieltes Debugging auf 1 setzen, im Normalbetrieb 0 lassen.
#define AUDIO_DEBUG_SERIAL 0

// --- I2C zum DevKitV1 (Taster-Abfrage + BT-Scan-Steuerung) ---
#define I2C_SDA         5    // D4, nativ SDA
#define I2C_SCL         6    // D5, nativ SCL
#define I2C_SLAVE_ADDR  0x42
#define I2C_CLOCK_HZ    50000
#define I2C_POLL_MS     80

// I2C-Kommandobytes – müssen exakt mit esp32_radio_bt/config.h übereinstimmen
#define I2C_CMD_BUTTONS        0x00
#define I2C_CMD_START_SCAN     0x01
#define I2C_CMD_SCAN_STATUS    0x02
#define I2C_CMD_SCAN_DEVICE    0x10
#define I2C_CMD_SET_BT_NAME    0x20
#define SCAN_MAX_DEVICES       8
#define SCAN_NAME_LEN          20
#define BT_NAME_MAX_LEN        32

// --- WLAN ---
#define STATIC_IP       "192.168.0.180"
#define GATEWAY         "192.168.0.254"
#define SUBNET          "255.255.255.0"
#define DNS_SERVER      "8.8.8.8"
#define MDNS_NAME       "esp32radio"
#define AP_SSID         "ESP32-Radio"
#define AP_PASS         "12345678"
#define WIFI_CONNECT_TIMEOUT_TRIES 30   // * 500ms
#define CAPTIVE_PORTAL_TIMEOUT_MS  180000

// --- Sender ---
#define STATION_COUNT 3

struct Station {
  String name;
  String url;
};

// Werden nur beim allerersten Start (kein NVS-Eintrag) verwendet.
static const Station DEFAULT_STATIONS[STATION_COUNT] = {
  {"Swiss Jazz",     "http://stream.srg-ssr.ch/m/rsj/mp3_128"},
  {"R. Caroline",    "http://sc6.radiocaroline.net:8040/listen.pls"},
  {"Absolute 60s",   "http://ais.absoluteradio.co.uk/absolute60s.mp3"}
};

// --- Phosphor-Grün Retro-Farben (RGB565) ---
#define PHOSPHOR_BG     0x0000
#define PHOSPHOR_DIM    0x0200
#define PHOSPHOR_MID    0x0580
#define PHOSPHOR_BRIGHT 0x07E0
#define PHOSPHOR_GLOW   0x03E0
