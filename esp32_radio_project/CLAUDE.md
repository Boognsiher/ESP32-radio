# ESP32 Internet-Radio – Projektauftrag für Claude Code

## Aufgabe

Implementiere den kompletten Sketch-Code für dieses Zwei-Board-Internetradio
**von Grund auf neu**. Dieses Dokument gibt die Hardware-Vorgaben, das
Architektur-Grundgerüst und bekannte Stolpersteine vor – der eigentliche
Code (Struktur, Funktionsaufteilung, Implementierungsdetails) soll von dir
frisch entworfen werden, nicht 1:1 aus alten Referenzen kopiert.

Falls im Ordner `reference/` alte Sketches liegen: die sind NUR zur
Orientierung bei den bekannten Stolpersteinen unten gedacht (z.B. exakte
Display-Init-Sequenz), nicht als Vorlage für die Gesamtstruktur.

**Zweiter Anlauf:** Die erste Umsetzung (Eigenbau-I2S/I2C-Code, siehe
`reference/`) ist an Audio-Rucklern und I2C-Instabilität gescheitert.
Ursachenanalyse anhand einer vergleichbaren, dokumentiert funktionierenden
Fremd-Implementierung (siehe Stolperstein #10) hat ergeben: fehlendes
Resampling, fehlendes PSRAM auf dem zweiten Board, und Debug-Serial-Output
während des Streams waren wahrscheinliche Hauptursachen. Diese Version
baut die Audio-Pipeline deshalb auf der Library `arduino-audio-tools`
(pschatzmann) statt auf Eigenbau-I2S-Code auf – siehe Stolpersteine
#10–#15.

---

## Projektziel

Internet-Radio mit:
- rundem 1.5" ST77916 QSPI-Display (360×360), Phosphor-Grün Retro-UI
- WLAN-Streaming von 3 konfigurierbaren Internetradio-Sendern
- Bluetooth-A2DP-Ausgabe an einen externen Lautsprecher
- 3 physischen Tastern zur Sender-Auswahl
- Webinterface: Sender-Konfiguration, Captive-Portal-WLAN-Setup,
  Bluetooth-Geräte-Scan mit Verbinden-Funktion

---

## Architektur (fest vorgegeben)

Zwei ESP32-Boards, weil nur der originale Xtensa-ESP32 klassisches
Bluetooth (A2DP) unterstützt – neuere Varianten (S3, C3, etc.) können
nur BLE.

```
Internet → WLAN → [Xiao ESP32-S3] --I2S--> [ESP32-WROVER-Board] --BT A2DP--> Lautsprecher
                   Display, Webinterface         Bluetooth-Bridge
                        ⇅ I2C (Taster + BT-Scan-Steuerung)
```

- **Xiao ESP32-S3** (8MB PSRAM onboard): WLAN, rundes Display,
  Webinterface, I2S-Sender, I2C-Master (fragt Taster/Scan-Ergebnisse
  vom zweiten Board ab)
- **ESP32-WROVER-Board** (PSRAM zwingend, siehe Stolperstein #11):
  I2S-Empfänger, Bluetooth-A2DP-Quelle, 3 Taster, I2C-Slave

**Wichtig – Boardwahl zweites Board:** Ein Standard-"ESP32 DevKitV1"
(WROOM-32-Modul) hat **kein PSRAM** und reicht für die nötigen
Audio-Puffer nicht zuverlässig aus (siehe Stolperstein #11). Es muss
ein Board mit ESP32-WROVER-Modul (oder gleichwertig, PSRAM onboard)
verwendet werden – Pinbelegung ist ansonsten identisch zu einem
DevKitV1-artigen Board.

Die 3 Taster hängen physisch am zweiten Board (nicht am S3), weil der
Xiao S3 nur 11 Header-Pins hat und Display+I2S bereits 9 davon belegen
– für 3 zusätzliche direkte Taster ist kein Platz. Diese Aufteilung ist
bewusst so und nicht zu ändern, ausser es gibt einen expliziten Auftrag.

**Audio-Pipeline (neu, verbindlich):** Beide Boards nutzen die Library
`arduino-audio-tools` (+ `ESP32-A2DP`, beide von pschatzmann) statt
Eigenbau-I2S-/A2DP-Code. Begründung und exakte Pipeline-Struktur siehe
Stolperstein #10. Kein rohes I2S ohne Resampling-Stufe.

---

## Hardware-Verdrahtung (fest vorgegeben, siehe auch `hardware/pinout.md`)

### Display (ST77916, QSPI) → Xiao ESP32-S3

| Signal | S3 Header-Pin | GPIO |
|--------|---------------|------|
| CS     | D9  | 8 |
| SCK    | D8  | 7 |
| IO0    | D10 | 9 |
| IO1    | D0  | 1 |
| IO2    | D1  | 2 |
| IO3    | D2  | 3 |
| RST    | –   | fest 3.3V (kein GPIO) |
| BLK    | –   | fest 3.3V (kein GPIO) |

### I2S: Xiao S3 → ESP32-WROVER-Board

| Funktion | S3 GPIO | WROVER GPIO |
|----------|---------|---------------|
| BCLK     | 4 (D3)  | 26 |
| LRCK     | 43 (D6) | 25 |
| DOUT     | 44 (D7) | 22 |

Übertragung über `I2SStream` (arduino-audio-tools) mit `ResampleStream`
auf 44.1kHz auf beiden Seiten – siehe Stolperstein #10.

### I2C: Xiao S3 ↔ ESP32-WROVER-Board

| Funktion | S3 GPIO | WROVER GPIO |
|----------|---------|---------------|
| SDA      | 5 (D4)  | 32 |
| SCL      | 6 (D5)  | 33 |

Slave-Adresse: `0x42`. Empfohlener Takt: 50kHz (100kHz zeigte in der
Vorgängerversion gelegentlich korrupte Mehrfach-Byte-Antworten bei
längeren Kabelstrecken – siehe Stolpersteine). **Vor weiterem
Timing-Debugging zuerst alle Lötstellen/Steckverbinder physisch auf
Wackelkontakt prüfen** (siehe Stolperstein #13).

### Taster → ESP32-WROVER-Board

| Funktion | GPIO | Pull-up |
|----------|------|---------|
| Sender 1 | 4    | intern (`INPUT_PULLUP`) |
| Sender 2 | 13   | intern (`INPUT_PULLUP`) |
| Sender 3 | 27   | intern (`INPUT_PULLUP`) |

### Stromversorgung

Externe 5V/2A-Quelle → S3 5V-Pin → Drahtbrücke → WROVER-Board
5V/VIN-Pin. GND gemeinsam. Nicht gleichzeitig USB und 5V-Brücke am
selben Board anschliessen (Rückspeisungskonflikt).

---

## Bekannte Stolpersteine (aus vorheriger Debugging-Session gelernt)

Diese Punkte haben in der Vorgängerversion konkrete Probleme verursacht
– bitte von Anfang an berücksichtigen, nicht erst nach Fehlschlägen:

1. **Display-Init**: Der `Arduino_ST77916`-Konstruktor (GFX Library for
   Arduino, moononournation) braucht zwingend Col/Row-Offsets UND eine
   explizite Init-Sequenz (`st77916_150_init_operations`, Teil der
   Library selbst). Ohne diese zeigt das Display nur Streifen/Bildreste.
   Referenz-Konstruktoraufruf falls vorhanden in `reference/`.

2. **Board-Package-Version**: ESP32-Board-Package 3.x verursacht Abstürze
   der Audio-Library (`StoreProhibited`-Panic). Version 2.0.x verwenden.

3. **Partitionsschema**: Standard-Partitionsschema reicht nicht für
   WLAN+WebServer+Audio+Display-Library zusammen. "Huge APP"-Schema
   oder vergleichbar grosszügig wählen.

4. **ESP32-A2DP-Library-API**: Neuere Versionen der Library
   (pschatzmann/ESP32-A2DP) heissen die Methode
   `set_on_connection_state_changed()`, nicht `on_connection_state_changed()`.

5. **Display-Refresh-Strategie**: Periodisches Vollbild-Neuzeichnen
   verursacht sichtbares Flackern auf dem runden Display. Ereignis-
   gesteuertes Update (nur bei Sender-/Titel-/WLAN-Änderung, dynamische
   Elemente wie Scroll-Text/Blink-Status nur zeilenweise neu zeichnen)
   hat das behoben.

6. **I2C-Zuverlässigkeit**: Bei einem isolierten Protokoll-Test lief die
   reine 2-Byte-Abfrage fehlerfrei, aber das Mehrfach-Transaktions-Muster
   (Kommando schreiben → Status abfragen → pro Gerät einzeln abfragen)
   zeigte gelegentlich korrupte Antworten. Ursache war zum Zeitpunkt des
   letzten Standes noch nicht abschliessend geklärt (Kandidaten: fehlende
   externe Pull-up-Widerstände auf SDA/SCL, oder ein Timing-Problem im
   Mehrfach-Kommando-Protokoll selbst). Baue Plausibilitätsprüfungen für
   I2C-Antworten ein (z.B. Statuswerte auf erwarteten Bereich prüfen,
   unplausible Antworten verwerfen statt anzuzeigen) und dokumentiere,
   falls du das Protokoll anders/robuster gestaltest als vorher.

7. **WLAN-Verbindungsaufbau über schwachen USB-Port**: Der anfängliche
   Verbindungsaufbau zu einem WLAN-Access-Point (auch der eigene
   Captive-Portal-Hotspot) kann bei einem PC-USB-Port mit nur ~500mA
   hängen bleiben ("Verbinde..." endlos). Kein Software-Fix nötig, aber
   gut zu wissen für den Testbetrieb.

8. **Fallback bei falschem WLAN-Passwort**: Wenn die Verbindung zum
   gespeicherten WLAN fehlschlägt, soll automatisch wieder der
   Captive-Portal-Hotspot geöffnet werden (kein Reflash/Serial-Befehl
   nötig, um die WLAN-Daten zu korrigieren).

9. **HTTPS-Streams**: können Klick-/Knack-Artefakte verursachen; HTTP-
   Streams bevorzugen, falls beides verfügbar ist.

10. **Audio-Pipeline auf `arduino-audio-tools` umstellen (zentrale Lehre
    aus dem gescheiterten ersten Anlauf)**: Ein öffentlich dokumentiertes
    Vergleichsprojekt mit identischem Ziel (WLAN-Radio → I2S zwischen
    zwei ESP32 → Bluetooth A2DP an Lautsprecher, siehe
    [pschatzmann/arduino-audio-tools Discussion #1748](https://github.com/pschatzmann/arduino-audio-tools/discussions/1748))
    hatte exakt unser Symptom (Ruckler alle ~200ms). Ursache dort:
    Sample-Rate-Mismatch zwischen Stream-Quelle (Sender liefern
    32/44.1/48kHz) und I2S-Übertragung. Fix: Resampling-Stufe auf
    44.1kHz auf **beiden** Boards. Verbindliche Pipeline für diese
    Version:
    ```
    Xiao S3:   URLStream → EncodedAudioStream(MP3DecoderHelix)
               → ResampleStream(→44.1kHz) → I2SStream (Master, TX)
    WROVER:    I2SStream (Slave, RX) → ResampleStream(48→44.1kHz)
               → A2DPStream → Bluetooth-Lautsprecher
    ```
    Kein Eigenbau-I2S-Code mehr für die Audio-Übertragung selbst
    (I2C-Steuerkanal für Taster/BT-Scan bleibt Eigenbau, siehe unten).

11. **PSRAM ist Pflicht auf beiden Boards**: Referenzprojekt (#10)
    brauchte Puffer von `buffer_size=1024*8, buffer_count=64`
    (~512KB) für rucklerfreie Wiedergabe – das übersteigt den internen
    SRAM eines ESP32 bei weiten (~300KB frei neben WLAN/BT-Stack).
    Xiao ESP32-S3 hat 8MB PSRAM onboard (aktivieren!). Für das zweite
    Board **kein** Standard-DevKitV1/WROOM-32 verwenden (kein PSRAM) –
    ein WROVER-Board (oder gleichwertig mit PSRAM) einsetzen, siehe
    Architektur-Abschnitt oben.

12. **Kein Serial-Debug-Output während aktivem Audio-Stream**: Im
    Referenzprojekt (#10) hat Serial-Debugging den Stream hörbar
    gestört. `Serial.print()`/`Serial.println()` im Audio-Hot-Path nur
    in einem expliziten Debug-/Testmodus aktivieren, nicht im
    Normalbetrieb.

13. **I2C-Instabilität (Stolperstein #6) zuerst auf Hardware prüfen**:
    Im Referenzprojekt (#10) stellte sich ein vermeintlicher
    Software-Bug (Knacken/Artefakte) als kalte Lötstelle heraus –
    Software und Hardware erzeugen ununterscheidbare Symptome. Vor
    erneutem Pull-up-/Timing-Debugging an der I2C-Leitung: alle
    Lötstellen und Steckverbindungen (I2C **und** I2S) physisch prüfen
    bzw. nachlöten.

14. **Definiertes Resync-Verhalten bei Senderwechsel**: Im
    Referenzprojekt (#10) erzeugte ein Neustart des Quell-Streams
    Sync-Glitches auf der Empfängerseite, teils nur durch Neustart des
    zweiten Boards behebbar. Da Senderwechsel bei uns über die 3
    Taster ein Kernfeature ist (nicht Ausnahmefall), braucht es ein
    explizites, koordiniertes Vorgehen beim Wechsel: I2S-Stream auf
    dem S3 sauber stoppen → kurze definierte Stille senden (kein
    unkontrollierter Cut) → neuen Stream starten; WROVER-Seite muss
    diesen Übergang erkennen und den Empfangspfad neu synchronisieren,
    ohne dass ein manueller Reboot nötig wird.

15. **Metadaten defensiv parsen**: Im Referenzprojekt (#10) haben
    Sonderzeichen (Kyrillisch) in Stream-Metadaten die Anzeige zum
    Absturz gebracht. Titel/Artist-Strings vor dem Rendern auf dem
    Display auf gültiges UTF-8 bzw. druckbare Zeichen validieren,
    unplausible/kaputte Metadaten verwerfen statt anzuzeigen (analog
    zum bestehenden Plausibilitätsprinzip aus Stolperstein #6).

---

## Funktionsanforderungen im Detail

### Xiao S3
- Verbindet sich mit gespeichertem WLAN (NVS), Fallback auf Captive
  Portal (SSID `ESP32-Radio`, Passwort `12345678`) bei fehlenden oder
  falschen Zugangsdaten
- Statische IP `192.168.0.180`, Gateway `192.168.0.254` (anpassbar)
- Streamt Audio von einer von 3 konfigurierbaren Sender-URLs, per I2S
  ans WROVER-Board
- Zeigt auf dem runden Display: Sender-Name, Artist/Titel (scrollend
  bei Überlänge), Status (verbindend/on air/Fehler), IP-Adresse
- Webinterface (Retro-Monospace-Stil, grün auf schwarz):
  - Hauptseite: aktueller Status, Sender-Liste mit Play-Buttons,
    Sender-Konfiguration (Name+URL, persistiert in NVS), WLAN-Reset,
    Neustart
  - BT-Scan-Seite: Scan starten, Ergebnisliste (Name/Adresse/RSSI),
    "Verbinden"-Button pro Gerät mit erkanntem Namen
- I2C-Master: fragt periodisch Taster-Status ab (Sender-Wechsel bei
  Änderung), löst BT-Scan aus, ruft Scan-Ergebnisse ab, sendet neuen
  BT-Zielnamen bei Klick auf "Verbinden"

### ESP32-WROVER-Board
- Empfängt Audio per I2S (Slave/RX) über `I2SStream` +
  `ResampleStream` (siehe Stolperstein #10), nicht per Eigenbau-I2S-Code
- Sendet per Bluetooth A2DP (`A2DPStream`, arduino-audio-tools) an
  konfigurierten Lautsprecher-Namen (persistiert in NVS, änderbar per
  Serial-Kommando `setbt:NAME` oder über die I2C-Schnittstelle vom S3)
- Auto-Reconnect bei Verbindungsabbruch
- Liest 3 Taster mit Entprellung
- I2C-Slave: beantwortet Taster-Status-Abfragen, führt BT-Geräte-Scan
  durch (klassische BT-Inquiry über ESP-IDF GAP-API) und liefert
  Ergebnisse, nimmt neuen BT-Zielnamen entgegen
- Serial-Kommandos (115200 Baud): `setbt:NAME`, `status`, `scan`

---

## Nicht Teil des Auftrags

- Keine Änderung der Zwei-Board-Architektur
- Keine Änderung der Pinbelegung ohne expliziten Auftrag
- Keine Segeluhr-Projektlogik – das ist ein separates Projekt, nur die
  Display-Init-Erkenntnisse sind hier relevant
