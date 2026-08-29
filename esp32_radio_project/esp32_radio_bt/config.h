// Zentrale Konfiguration – ESP32 DevKitV1 (Bluetooth-Bridge).
// Pin-Zuordnung siehe hardware/pinout.md und CLAUDE.md.
#pragma once

// --- I2S vom Xiao S3 (DevKit ist Slave, S3 liefert den Takt) ---
#define I2S_BCLK        26
#define I2S_LRCK        25
#define I2S_DIN         22
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS    2
#define AUDIO_BITS        16

// Stolperstein #12: Serial-Debug im Audio-Hot-Path stört hörbar den Stream.
#define AUDIO_DEBUG_SERIAL 0

// --- I2C-Slave (Taster-Meldung + BT-Scan-Abfrage vom S3) ---
#define I2C_SDA         32
#define I2C_SCL         33
#define I2C_SLAVE_ADDR  0x42
#define I2C_CLOCK_HZ    50000

// I2C-Kommandobytes – müssen exakt mit esp32_radio_s3/config.h übereinstimmen
#define I2C_CMD_BUTTONS        0x00
#define I2C_CMD_START_SCAN     0x01
#define I2C_CMD_SCAN_STATUS    0x02
#define I2C_CMD_SCAN_DEVICE    0x10
#define I2C_CMD_SET_BT_NAME    0x20
#define SCAN_MAX_DEVICES       8
#define SCAN_NAME_LEN          20
#define BT_NAME_MAX_LEN        32

// --- Taster (gegen GND, interner Pull-up) ---
#define BTN_1           4
#define BTN_2           13
#define BTN_3           27
#define STATION_COUNT   3
#define DEBOUNCE_MS     400

// --- Bluetooth ---
#define BT_NAME_DEFAULT "Bose"
#define BT_SCAN_DURATION_UNITS 10   // * 1.28s ~= 12.8s
