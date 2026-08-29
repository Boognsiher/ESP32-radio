# Pinout-Referenz

Zweites Board aktuell: **ESP32 DevKitV1** (vorhandene Hardware, kein
PSRAM). Falls trotz Resampling (Stolperstein #10 in `CLAUDE.md`)
weiterhin Ruckler auftreten, ist ein Wechsel auf ein PSRAM-Board
(ESP32-WROVER) der nächste Schritt – Pinbelegung unten bliebe dabei
identisch. Das vorhandene "ESP32-S3 Mini" ist **keine** Alternative
dafür (kein klassisches Bluetooth A2DP, nur BLE).

## Display (ST77916, rund, QSPI, 360×360, "TM093") → Xiao ESP32-S3

| Display-Beschriftung | Bedeutung | S3 Header-Pin | GPIO |
|---|---|---|---|
| CS  | Chip Select | D9  | 8 |
| SCL | Clock (=SCK)| D8  | 7 |
| SDA | Daten 0 (=IO0) | D10 | 9 |
| IO1 | Daten 1 | D0 | 1 |
| IO2 | Daten 2 | D1 | 2 |
| IO3 | Daten 3 | D2 | 3 |
| RST | Reset | – | fest 3.3V, kein GPIO |
| BL  | Backlight | – | fest 3.3V, kein GPIO |
| VCC | – | – | 3.3V |
| GND | – | – | GND |

## I2S (Audio): Xiao S3 → ESP32 DevKitV1

| Funktion | S3 Header-Pin | S3 GPIO | DevKitV1 GPIO |
|---|---|---|---|
| BCLK | D3 | 4  | 26 |
| LRCK | D6 | 43 | 25 |
| DOUT | D7 | 44 | 22 |
| GND  | –  | GND | GND |

Übertragen wird via `I2SStream` aus `arduino-audio-tools`, mit
`ResampleStream` auf beiden Seiten auf 44.1kHz normalisiert (siehe
Stolperstein #10 in `CLAUDE.md`) – kein rohes I2S ohne Resampling mehr.

## I2C (Taster + BT-Scan-Steuerung): Xiao S3 ↔ ESP32 DevKitV1

| Funktion | S3 Header-Pin | S3 GPIO | DevKitV1 GPIO |
|---|---|---|---|
| SDA | D4 | 5 | 32 |
| SCL | D5 | 6 | 33 |

Slave-Adresse `0x42`, empfohlener Takt 50kHz. Externe 4.7kΩ-Pull-ups auf
SDA/SCL nach 3.3V empfohlen (siehe Stolperstein #6 in `CLAUDE.md`).
**Vor erneutem Timing-/Pull-up-Debugging: alle Lötstellen/Steckverbinder
der I2C-Leitung physisch auf Wackelkontakt prüfen** (siehe Stolperstein
#13 – in einem vergleichbaren Projekt war ein vermeintlicher
Software-Bug tatsächlich eine kalte Lötstelle).

## Taster → ESP32 DevKitV1 (gegen GND)

| Funktion | GPIO | Pull-up |
|---|---|---|
| Sender 1 | 4  | intern (`INPUT_PULLUP`) |
| Sender 2 | 13 | intern (`INPUT_PULLUP`) |
| Sender 3 | 27 | intern (`INPUT_PULLUP`) |

## Pin-Bilanz Xiao S3

Alle 11 Header-Pins (D0–D10) belegt: Display 6 + I2S 3 + I2C 2 = 11.
Kein Pin mehr frei.

## Stromversorgung

| Von | Nach | Hinweis |
|---|---|---|
| Externe 5V/2A-Quelle | Xiao S3 5V-Pin | Hauptversorgung |
| S3 5V-Pin | DevKitV1 5V/VIN-Pin | Drahtbrücke |
| GND | gemeinsam | zwischen beiden Boards verbunden |

Nicht gleichzeitig USB und 5V-Brücke am selben Board anschliessen.
