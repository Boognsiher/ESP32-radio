# ESP32 Internet-Radio

Zwei-Board-Internetradio: Xiao ESP32-S3 (WLAN, rundes Display, Webinterface)
+ ESP32 DevKitV1 (Bluetooth A2DP, Taster) über I2S (Audio) und I2C
(Steuerung) verbunden. Audio-Pipeline auf beiden Boards basiert auf der
Library `arduino-audio-tools` (inkl. Resampling) statt auf Eigenbau-Code
– siehe Begründung in `CLAUDE.md`.

## Status

Zweiter Anlauf ist implementiert (`esp32_radio_s3/`, `esp32_radio_bt/`),
nachdem die erste Version an Audio-Rucklern und I2C-Instabilität
gescheitert ist (siehe "Stolpersteine" in `CLAUDE.md`). Die Spezifikation
(Hardware, Architektur, bekannte Stolpersteine) steht in [`CLAUDE.md`](./CLAUDE.md)
und [`hardware/pinout.md`](./hardware/pinout.md) – das ist die
verbindliche Vorgabe.

**Wichtig – noch nicht kompiliert/getestet:** Der Code wurde anhand
verifizierter Beispiele aus der offiziellen `arduino-audio-tools`-
Dokumentation und dem GitHub-Repo geschrieben, konnte in dieser Umgebung
aber **nicht selbst kompiliert werden** (Netzwerk-Policy blockiert den
Download von Arduino-/PlatformIO-Toolchains). Bitte beim ersten Compile
in der Arduino IDE auftretende Fehler (v.a. exakte API-Feldnamen wie
`ResampleConfig::to_sample_rate`, `EncodedAudioStream::addNotifyAudioChange`,
`URLStream`-Konstruktor/`setMetadataCallback`) gegen die tatsächlich
installierte Library-Version prüfen – das sind die wahrscheinlichsten
Stellen für kleinere Anpassungen.

**Vorhandene Boards:** Xiao ESP32-S3 + ESP32 DevKitV1 (kein PSRAM). Ein
"ESP32-S3 Mini" ist ebenfalls vorhanden, aber für die BT-Rolle **nicht**
geeignet (S3 kann kein klassisches Bluetooth A2DP). Falls das DevKitV1
trotz Resampling weiter ruckelt, ist ein PSRAM-Board (ESP32-WROVER) als
Ersatz der dokumentierte nächste Schritt.

## Struktur

```
esp32_radio_s3/    ← Sketch für Xiao ESP32-S3
esp32_radio_bt/    ← Sketch für ESP32 DevKitV1
hardware/          ← Pinout, Verkabelung, Fotos/Diagramme
reference/         ← Alte Sketches nur zur Orientierung (nicht 1:1 übernehmen)
```

## Build

- Xiao ESP32-S3: Board `XIAO_ESP32S3`, ESP32-Board-Package **2.0.x**,
  Partition Scheme "Huge APP", PSRAM **aktiviert** (Board hat 8MB onboard)
- ESP32 DevKitV1: Board `ESP32 Dev Module`, ESP32-Board-Package **2.0.x**
  – kein PSRAM, daher kleinere Audio-Puffer als im Referenzprojekt
  (siehe Stolperstein #11 in `CLAUDE.md`)

### Benötigte Libraries (Library Manager, beide Boards)

| Library | Autor | Zweck |
|---|---|---|
| GFX Library for Arduino | moononournation | Display (nur `esp32_radio_s3`) |
| arduino-audio-tools | pschatzmann | Audio-Pipeline (I2S/URL/Resample/A2DP) |
| arduino-libhelix | pschatzmann | MP3-Decoder, Abhängigkeit von arduino-audio-tools |
| ESP32-A2DP | pschatzmann | Bluetooth-A2DP-Backend, Abhängigkeit von arduino-audio-tools |

Details siehe `CLAUDE.md`.
