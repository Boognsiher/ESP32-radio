# ESP32 Internet-Radio

Zwei-Board-Internetradio: Xiao ESP32-S3 (WLAN, rundes Display, Webinterface)
+ ESP32-WROVER-Board mit PSRAM (Bluetooth A2DP, Taster) über I2S (Audio)
und I2C (Steuerung) verbunden. Audio-Pipeline auf beiden Boards basiert
auf der Library `arduino-audio-tools` (inkl. Resampling) statt auf
Eigenbau-Code – siehe Begründung in `CLAUDE.md`.

## Status

Projekt wird aktuell mit Claude Code neu aufgebaut, **zweiter Anlauf**
nach einer ersten Version, die an Audio-Rucklern und I2C-Instabilität
gescheitert ist (siehe "Stolpersteine" in `CLAUDE.md`). Die Spezifikation
(Hardware, Architektur, bekannte Stolpersteine) steht in [`CLAUDE.md`](./CLAUDE.md)
und [`hardware/pinout.md`](./hardware/pinout.md) – das ist die
verbindliche Vorgabe, der Code selbst wird von Grund auf neu geschrieben.

## Struktur

```
esp32_radio_s3/    ← Sketch für Xiao ESP32-S3
esp32_radio_bt/    ← Sketch für ESP32-WROVER-Board
hardware/          ← Pinout, Verkabelung, Fotos/Diagramme
reference/         ← Alte Sketches nur zur Orientierung (nicht 1:1 übernehmen)
```

## Build

- Xiao ESP32-S3: Board `XIAO_ESP32S3`, ESP32-Board-Package **2.0.x**,
  Partition Scheme "Huge APP", PSRAM **aktiviert** (Board hat 8MB onboard)
- ESP32-WROVER-Board: Board `ESP32 Wrover Module` (oder gleichwertiges
  Board mit PSRAM, **kein** Standard-DevKitV1/WROOM-32 ohne PSRAM!),
  ESP32-Board-Package **2.0.x**, PSRAM **aktiviert**
- Beide Boards: Library `arduino-audio-tools` (pschatzmann) +
  `ESP32-A2DP` (pschatzmann) als Abhängigkeit

Details siehe `CLAUDE.md`.
