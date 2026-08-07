# ESP32 Internet-Radio

Zwei-Board-Internetradio: Xiao ESP32-S3 (WLAN, rundes Display, Webinterface)
+ ESP32 DevKitV1 (Bluetooth A2DP, Taster) über I2S (Audio) und I2C
(Steuerung) verbunden.

## Status

Projekt wird aktuell mit Claude Code neu aufgebaut. Die Spezifikation
(Hardware, Architektur, bekannte Stolpersteine) steht in [`CLAUDE.md`](./CLAUDE.md)
und [`hardware/pinout.md`](./hardware/pinout.md) – das ist die
verbindliche Vorgabe, der Code selbst wird von Grund auf neu geschrieben.

## Struktur

```
esp32_radio_s3/    ← Sketch für Xiao ESP32-S3
esp32_radio_bt/    ← Sketch für ESP32 DevKitV1
hardware/          ← Pinout, Verkabelung, Fotos/Diagramme
reference/         ← Alte Sketches nur zur Orientierung (nicht 1:1 übernehmen)
```

## Build

- Xiao ESP32-S3: Board `XIAO_ESP32S3`, ESP32-Board-Package **2.0.x**,
  Partition Scheme "Huge APP"
- ESP32 DevKitV1: Board `ESP32 Dev Module`, ESP32-Board-Package **2.0.x**

Details siehe `CLAUDE.md`.
