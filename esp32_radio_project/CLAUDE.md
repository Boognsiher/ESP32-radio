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
Internet → WLAN → [Xiao ESP32-S3] --I2S--> [ESP32 DevKitV1] --BT A2DP--> Lautsprecher
                   Display, Webinterface         Bluetooth-Bridge
                        ⇅ I2C (Taster + BT-Scan-Steuerung)
```

- **Xiao ESP32-S3**: WLAN, rundes Display, Webinterface, I2S-Sender,
  I2C-Master (fragt Taster/Scan-Ergebnisse vom DevKit ab)
- **ESP32 DevKitV1**: I2S-Empfänger, Bluetooth-A2DP-Quelle, 3 Taster,
  I2C-Slave

Die 3 Taster hängen physisch am DevKit (nicht am S3), weil der Xiao S3
nur 11 Header-Pins hat und Display+I2S bereits 9 davon belegen – für
3 zusätzliche direkte Taster ist kein Platz. Diese Aufteilung ist
bewusst so und nicht zu ändern, ausser es gibt einen expliziten Auftrag.

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

### I2S: Xiao S3 → DevKitV1

| Funktion | S3 GPIO | DevKitV1 GPIO |
|----------|---------|---------------|
| BCLK     | 4 (D3)  | 26 |
| LRCK     | 43 (D6) | 25 |
| DOUT     | 44 (D7) | 22 |

### I2C: Xiao S3 ↔ DevKitV1

| Funktion | S3 GPIO | DevKitV1 GPIO |
|----------|---------|---------------|
| SDA      | 5 (D4)  | 32 |
| SCL      | 6 (D5)  | 33 |

Slave-Adresse: `0x42`. Empfohlener Takt: 50kHz (100kHz zeigte in der
Vorgängerversion gelegentlich korrupte Mehrfach-Byte-Antworten bei
längeren Kabelstrecken – siehe Stolpersteine).

### Taster → DevKitV1

| Funktion | GPIO | Pull-up |
|----------|------|---------|
| Sender 1 | 4    | intern (`INPUT_PULLUP`) |
| Sender 2 | 13   | intern (`INPUT_PULLUP`) |
| Sender 3 | 27   | intern (`INPUT_PULLUP`) |

### Stromversorgung

Externe 5V/2A-Quelle → S3 5V-Pin → Drahtbrücke → DevKitV1 5V/VIN-Pin.
GND gemeinsam. Nicht gleichzeitig USB und 5V-Brücke am selben Board
anschliessen (Rückspeisungskonflikt).

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

---

## Funktionsanforderungen im Detail

### Xiao S3
- Verbindet sich mit gespeichertem WLAN (NVS), Fallback auf Captive
  Portal (SSID `ESP32-Radio`, Passwort `12345678`) bei fehlenden oder
  falschen Zugangsdaten
- Statische IP `192.168.0.180`, Gateway `192.168.0.254` (anpassbar)
- Streamt Audio von einer von 3 konfigurierbaren Sender-URLs, per I2S
  ans DevKit
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

### DevKitV1
- Empfängt Audio per I2S (Slave/RX)
- Sendet per Bluetooth A2DP an konfigurierten Lautsprecher-Namen
  (persistiert in NVS, änderbar per Serial-Kommando `setbt:NAME` oder
  über die I2C-Schnittstelle vom S3)
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
