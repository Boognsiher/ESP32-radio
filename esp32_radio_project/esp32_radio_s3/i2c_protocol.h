#pragma once
// ═══════════════════════════════════════════════════════════════
// Gemeinsames I2C-Protokoll: Xiao S3 (Master) <-> DevKitV1 (Slave)
// ═══════════════════════════════════════════════════════════════
// WICHTIG: Diese Datei liegt identisch in beiden Sketch-Ordnern
// (esp32_radio_s3/ und esp32_radio_bt/), da Arduino-Sketches keine
// Header ausserhalb ihres eigenen Ordners einbinden können. Bei
// Protokolländerungen IMMER beide Kopien synchron halten.
//
// Ablauf pro Abfrage: der Master schreibt 1 Kommando-Byte (bei
// I2C_CMD_GET_BUTTONS optional, das ist der Default-Zustand nach
// jeder Antwort), danach liest er per requestFrom() die passende
// Antwort. Kommandos ohne Antwort (START_SCAN, SET_BT_TARGET) sind
// reine Wire.write()-Transaktionen ohne anschliessendes requestFrom().
//
// Jede Mehrbyte-Antwort trägt am Ende eine XOR-Checksumme (siehe
// i2cChecksum()). Das ist keine Kryptographie, sondern eine simple
// Plausibilitätsprüfung gegen die in CLAUDE.md (Stolperstein #6)
// beschriebenen gelegentlichen Bit-Fehler bei Mehrfach-Transaktionen.
// Beide Seiten verwerfen Antworten mit falscher Checksumme (bzw.
// unplausiblen Wertebereichen) statt sie zu verwenden -- das ist die
// hier gewählte, robustere Variante gegenüber der Vorgängerversion.

#include <stdint.h>
#include <stddef.h>

#define I2C_SLAVE_ADDR            0x42
#define I2C_CLOCK_HZ               50000   // siehe CLAUDE.md Stolperstein #6

#define I2C_CMD_GET_BUTTONS        0x00   // Antwort: [event, stationIdx, btConnected, chk] (4 Byte).
                                           // stationIdx ist normalerweise 0..STATION_COUNT-1;
                                           // der Sonderwert I2C_BTN_EVENT_SHOW_IP signalisiert
                                           // statt Senderwechsel die Taster-Kombi 1+2 (>=1s
                                           // gehalten) fürs kurzzeitige IP-Anzeigen -- siehe
                                           // buttons.cpp (DevKit) / radio.cpp (S3). btConnected
                                           // ist auf dieser Standard-Antwort mit draufgepackt
                                           // (statt eines eigenen GET_BT_STATUS-Kommandos):
                                           // Hardware-Test zeigte, dass das "Kommando schreiben,
                                           // dann separat requestFrom()"-Muster bei diesem Slave
                                           // zuverlässig fehlschlägt (immer 0x00-Bytes statt der
                                           // echten Antwort, unabhängig von Timing/Pull-ups/
                                           // repeated-start -- Ursache nicht abschliessend
                                           // geklärt). GET_BUTTONS ist das einzige nachweislich
                                           // zuverlässige Muster (nie ein vorheriges sendCommand
                                           // nötig, Slave antwortet immer mit dem Default-Zustand),
                                           // deshalb btConnected hier huckepack statt über ein
                                           // eigenes Kommando.
#define I2C_CMD_START_SCAN         0x01   // kein Antwort-Request
#define I2C_CMD_GET_SCAN_STATUS    0x02   // Antwort: [state, count, chk]       (3 Byte)
#define I2C_CMD_GET_ROOM_TEMP      0x03   // Antwort: [tempLo, tempHi, valid, chk] (4 Byte).
                                           // temp = int16, 0.1°C-Einheiten (z.B. 235 = 23.5°C,
                                           // -52 = -5.2°C). valid=0 -> Sensor fehlt/Fehler,
                                           // tempLo/Hi dann ignorieren.
#define I2C_CMD_GET_SCAN_DEVICE    0x10   // + Index 0..7,  Antwort: Scan-Record (s.u.)
#define I2C_CMD_SET_BT_TARGET      0x20   // + Namensbytes, kein Antwort-Request
#define I2C_CMD_SET_BT_MAC         0x21   // + 6 Rohbytes MAC-Adresse, kein Antwort-Request.
                                           // Payload leer (0 Byte) statt 6 Byte -> Slave
                                           // interpretiert das als "feste MAC entfernen,
                                           // zurück auf Namens-Verbindung".

#define I2C_BTN_EVENT_SHOW_IP      0xFF   // Sonderwert für stationIdx, siehe I2C_CMD_GET_BUTTONS oben

#define I2C_SCAN_MAX_DEVICES       8
#define I2C_SCAN_NAME_LEN          20
#define I2C_BT_NAME_MAXLEN         32

#define I2C_SCAN_STATE_IDLE        0
#define I2C_SCAN_STATE_RUNNING     1
#define I2C_SCAN_STATE_DONE        2

// Scan-Record-Layout: name(20) + bdAddr(6) + rssi(1) + checksum(1)
#define I2C_SCAN_RECORD_LEN        (I2C_SCAN_NAME_LEN + 6 + 1 + 1)

#define I2C_CHECKSUM_SALT          0xA5

inline uint8_t i2cChecksum(const uint8_t *data, size_t len) {
  uint8_t sum = I2C_CHECKSUM_SALT;
  for (size_t i = 0; i < len; i++) sum ^= data[i];
  return sum;
}
