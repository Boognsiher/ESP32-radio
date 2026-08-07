#pragma once
// Pin- und Netzwerk-Konfiguration Xiao ESP32-S3 -- siehe hardware/pinout.md
// und CLAUDE.md für die verbindliche Vorgabe. Nicht ohne expliziten
// Auftrag ändern.

// --- Display (ST77916, QSPI, 360x360 rund) ---
#define PIN_TFT_CS    8   // D9
#define PIN_TFT_SCK   7   // D8
#define PIN_TFT_D0    9   // D10 (IO0)
#define PIN_TFT_D1    1   // D0  (IO1)
#define PIN_TFT_D2    2   // D1  (IO2)
#define PIN_TFT_D3    3   // D2  (IO3)
#define PIN_TFT_RST   -1  // RST fest 3.3V verdrahtet, kein GPIO
#define TFT_WIDTH     360
#define TFT_HEIGHT    360
#define TFT_ROTATION  0
#define TFT_IPS       true

// --- I2S Ausgang zum DevKitV1 ---
#define PIN_I2S_BCLK  4    // D3
#define PIN_I2S_LRCK  43   // D6
#define PIN_I2S_DOUT  44   // D7

// --- I2C Master zum DevKitV1 (Taster-Abfrage + BT-Scan-Steuerung) ---
#define PIN_I2C_SDA   5    // D4
#define PIN_I2C_SCL   6    // D5
#define I2C_POLL_INTERVAL_MS   80

// --- WLAN ---
#define WIFI_STATIC_IP    "192.168.0.180"
#define WIFI_GATEWAY      "192.168.0.254"
#define WIFI_SUBNET       "255.255.255.0"
#define WIFI_DNS          "8.8.8.8"
#define WIFI_AP_SSID      "ESP32-Radio"
#define WIFI_AP_PASS      "12345678"
#define WIFI_CONNECT_TIMEOUT_MS    15000
#define CAPTIVE_PORTAL_TIMEOUT_MS  180000
#define MDNS_HOSTNAME     "esp32radio"

#define STATION_COUNT 3

// --- Wetter (Open-Meteo, kostenlos, kein API-Key) ---
#define WEATHER_API_HOST      "api.open-meteo.com"
// Default-Standort Zürich -- im Webinterface auf den echten Standort
// (z.B. Wohnort der Grosseltern) umstellbar, NVS-persistiert.
#define WEATHER_DEFAULT_LAT   47.3769f
#define WEATHER_DEFAULT_LON   8.5417f
#define WEATHER_FETCH_INTERVAL_MS   (20UL * 60UL * 1000UL)   // alle 20 Minuten
#define WEATHER_HTTP_TIMEOUT_MS     8000

// --- Anzeige-Rotation: Radio-Screen <-> Wetter-Screen ---
#define SCREEN_RADIO_DURATION_MS    (12UL * 1000UL)
#define SCREEN_WEATHER_DURATION_MS  (6UL * 1000UL)

// Wie oft die Raumtemperatur per I2C vom DevKit abgefragt wird -- an
// dessen eigenes Mess-Intervall angelehnt (ROOM_TEMP_POLL_INTERVAL_MS in
// esp32_radio_bt/config.h), häufigeres Abfragen brächte nichts.
#define ROOM_TEMP_I2C_POLL_MS  20000
