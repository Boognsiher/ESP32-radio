#include "i2s_audio.h"
#include "config.h"
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>

namespace I2sAudio {

// Versionshinweis: der Xiao S3 nutzt jetzt ESP32-audioI2S 2.0.0 (siehe
// display.cpp-Kommentar zur Board-Package-/Bibliotheksversion-Umstellung
// -- die neuere 3.4.7 blockierte bei bestimmten Streaming-Servern
// minutenlang beim HTTP-Header-Parsing). 2.0.0 konfiguriert ihr I2S
// intern mit I2S_BITS_PER_SAMPLE_16BIT (echte 16-Bit-Slots, kein
// 32-Bit-Padding wie bei 3.4.7) -- der RX-Treiber hier MUSS exakt
// dieselbe Slot-Breite erwarten (Slave-Modus: er zählt Bits nur relativ
// zu WS/BCLK vom Master mit, generiert kein eigenes Timing). Deshalb
// hier bewusst wieder auf 16-Bit/Slot mit direktem Read, kein 32-Bit-
// Zwischenpuffer/Extraktion mehr nötig.
void begin() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = I2S_DMA_BUF_COUNT,
    .dma_buf_len = I2S_DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num = PIN_I2S_BCLK,
    .ws_io_num = PIN_I2S_LRCK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = PIN_I2S_DIN
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
  Serial.println("[I2S] Initialisiert (RX/Slave, 16-Bit-Slots)");
}

size_t readFrames(int16_t *outLR, size_t frameCount) {
  // WICHTIG: NIE portMAX_DELAY hier. Dieser Aufruf laeuft im Kontext des
  // A2DP-dataCallback(), der wiederum auf dem Bluetooth-Controller-Task
  // (BTC) des Stacks ausgefuehrt wird. Steht der S3 (I2S-Master) auch nur
  // kurz still (Netzwerk-Hickup, Buffer-Underrun beim Streamen), lieferte
  // er kein BCLK/WS mehr -- i2s_read() blockierte dann mit portMAX_DELAY
  // unbegrenzt und blockierte damit den kompletten BTC-Task. Der Speaker
  // sieht daraufhin keine Supervision-Antworten mehr und trennt die
  // Verbindung ("verbunden -> Started -> nach 1-2 Callbacks Getrennt",
  // exakt das beobachtete Hardware-Verhalten). Fix: kurzes, begrenztes
  // Timeout -- bei fehlenden Daten liefert i2s_read() dann bytesRead=0,
  // dataCallback() erkennt got==0 bereits und fuellt Stille (siehe
  // bt_a2dp.cpp), statt den Stack einzufrieren.
  size_t bytesRead = 0;
  i2s_read(I2S_NUM_0, outLR, frameCount * 2 * sizeof(int16_t), &bytesRead, pdMS_TO_TICKS(5));
  return bytesRead / (2 * sizeof(int16_t));
}

} // namespace I2sAudio
