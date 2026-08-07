# ESP32 Internet-Radio

Zwei-Board-Internetradio: Xiao ESP32-S3 (WLAN, rundes Display, Webinterface)
+ ESP32 DevKitV1 (Bluetooth A2DP, Taster) über I2S (Audio) und I2C
(Steuerung) verbunden.

## Status

Der Code wurde gemäss [`CLAUDE.md`](./CLAUDE.md) von Grund auf neu
implementiert (modulare Struktur, frisch entworfen – nicht 1:1 aus
`reference/` übernommen). `CLAUDE.md` und
[`hardware/pinout.md`](./hardware/pinout.md) bleiben die verbindliche
Vorgabe für Hardware/Architektur.

## Struktur

```
esp32_radio_s3/    ← Sketch für Xiao ESP32-S3 (WLAN, Display, Webinterface, I2S-TX, I2C-Master)
esp32_radio_bt/    ← Sketch für ESP32 DevKitV1 (I2S-RX, BT-A2DP, Taster, I2C-Slave)
hardware/          ← Pinout, Verkabelung, Fotos/Diagramme
reference/         ← Alte Sketches nur zur Orientierung (nicht 1:1 übernehmen)
```

Beide Sketches sind je in kleine, fachlich getrennte Module aufgeteilt
(config.h + je ein .h/.cpp-Paar pro Verantwortlichkeit), die vom jeweiligen
`.ino` nur noch orchestriert werden:

| esp32_radio_s3/           | esp32_radio_bt/         |
|----------------------------|--------------------------|
| `display.*` – Phosphor-Grün-UI, ereignisgesteuertes Redraw | `i2s_audio.*` – I2S-Empfang (Slave/RX) |
| `stations.*` – Sender-Liste, NVS-Persistenz | `bt_a2dp.*` – A2DP-Quelle, Auto-Reconnect |
| `audio_stream.*` – MP3-Decoding + I2S-Ausgabe | `buttons.*` – Taster, Entprellung |
| `wifi_manager.*` – WLAN-Connect + Captive Portal | `bt_scan.*` – klassischer BT-Inquiry-Scan |
| `i2c_master.*` – I2C-Protokoll, Master-Seite | `i2c_slave.*` – I2C-Protokoll, Slave-Seite |
| `radio.*` – zentraler Zustand/Koordination | `serial_console.*` – Serial-Kommandos |
| `web_server.*` – Retro-Webinterface | |

`i2c_protocol.h` liegt identisch in beiden Sketch-Ordnern (Arduino kann
keine Header ausserhalb des Sketch-Ordners einbinden) und definiert das
gemeinsame I2C-Kommando-Layout. Gegenüber der Vorgängerversion in
`reference/` trägt jede Mehrbyte-I2C-Antwort zusätzlich eine XOR-
Checksumme; beide Seiten verwerfen Antworten mit falscher Checksumme oder
unplausiblem Wertebereich, statt sie zu verwenden (Umsetzung von CLAUDE.md
Stolperstein #6).

### Bluetooth-Ziel: Name vs. feste MAC-Adresse

Der Lautsprecher lässt sich entweder per Name (klassische Discovery-
Suche) oder per fester MAC-Adresse verbinden. Die MAC-Variante ist
zuverlässiger, da `BluetoothA2DPSource` dabei laut Library-Quellcode
(`set_auto_reconnect(esp_bd_addr_t, retries)` + `start()` ohne Namen)
direkt verbindet, ohne vorherigen Discovery-Scan. Eine gesetzte MAC hat
Vorrang vor dem Namen.

- **Webinterface** (`/btscan`): "VERBINDEN" bei einem Scan-Ergebnis nutzt
  automatisch dessen MAC-Adresse; alternativ lässt sich eine bekannte MAC
  auch direkt eintragen. "MAC ENTFERNEN" schaltet zurück auf Namenssuche.
- **Serial** (DevKitV1, 115200 Baud): `setbtmac:AA:BB:CC:DD:EE:FF` /
  `clearbtmac` (zusätzlich zu `setbt:NAME`, `scan`, `status`).

## Benötigte Libraries

- **GFX Library for Arduino** (moononournation) – Display-Treiber
- **ESP32-audioI2S** (schreibfaul2) – MP3-Streaming/I2S-Ausgabe (Xiao S3)
- **ESP32-A2DP** (pschatzmann) – Bluetooth-A2DP-Quelle (DevKitV1)
- WiFi, WebServer, DNSServer, ESPmDNS, Preferences, Wire – Teil des
  ESP32-Board-Packages

## Build

- Xiao ESP32-S3: Board `XIAO_ESP32S3`, ESP32-Board-Package **2.0.x**,
  Partition Scheme "Huge APP"
- ESP32 DevKitV1: Board `ESP32 Dev Module`, ESP32-Board-Package **2.0.x**

Details und bekannte Stolpersteine siehe `CLAUDE.md`.
