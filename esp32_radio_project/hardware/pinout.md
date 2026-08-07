# Pinout-Referenz

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

## I2S (Audio): Xiao S3 → DevKitV1

| Funktion | S3 Header-Pin | S3 GPIO | DevKitV1 GPIO |
|---|---|---|---|
| BCLK | D3 | 4  | 26 |
| LRCK | D6 | 43 | 25 |
| DOUT | D7 | 44 | 22 |
| GND  | –  | GND | GND |

## I2C (Taster + BT-Scan-Steuerung): Xiao S3 ↔ DevKitV1

| Funktion | S3 Header-Pin | S3 GPIO | DevKitV1 GPIO |
|---|---|---|---|
| SDA | D4 | 5 | 32 |
| SCL | D5 | 6 | 33 |

Slave-Adresse `0x42`, empfohlener Takt 50kHz. Externe 4.7kΩ-Pull-ups auf
SDA/SCL nach 3.3V empfohlen (siehe Stolperstein #6 in `CLAUDE.md`).

## Taster → DevKitV1 (gegen GND)

| Funktion | GPIO | Pull-up |
|---|---|---|
| Sender 1 | 4  | intern (`INPUT_PULLUP`) |
| Sender 2 | 13 | intern (`INPUT_PULLUP`) |
| Sender 3 | 27 | intern (`INPUT_PULLUP`) |

## Raumtemperatur-Sensor (DS18B20, 1-Wire, optional) → DevKitV1

| Funktion | GPIO | Hinweis |
|---|---|---|
| DATA | 14 | + externer 4.7kΩ-Pull-up nach 3.3V (Standard bei 1-Wire) |
| VCC  | – | 3.3V |
| GND  | – | GND |

Kein Pflichtbestandteil: ohne angeschlossenen Sensor meldet das DevKit
per I2C einfach "kein gültiger Wert", der S3 zeigt dann "n/a" auf dem
Wetter-Bildschirm. GPIO 14 ist frei und keine Strapping-Pin-Problematik
(anders als GPIO 0/2/5/12/15).

## Pin-Bilanz

**Xiao S3**: Alle 11 Header-Pins (D0–D10) belegt: Display 6 + I2S 3 +
I2C 2 = 11. Kein Pin mehr frei -- zusätzliche Sensoren nur am DevKit
möglich, nicht am S3 (siehe CLAUDE.md).

**DevKitV1**: aktuell 9 von ca. 25 nutzbaren GPIOs belegt (I2S 3, I2C 2,
Taster 3, Temperatursensor 1) -- reichlich Reserve für weitere Sensoren.

## Stromversorgung

| Von | Nach | Hinweis |
|---|---|---|
| Externe 5V/2A-Quelle | Xiao S3 5V-Pin | Hauptversorgung |
| S3 5V-Pin | DevKitV1 5V/VIN-Pin | Drahtbrücke |
| GND | gemeinsam | zwischen beiden Boards verbunden |

Nicht gleichzeitig USB und 5V-Brücke am selben Board anschliessen.
