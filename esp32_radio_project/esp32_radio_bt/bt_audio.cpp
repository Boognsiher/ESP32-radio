#include "bt_audio.h"
#include "config.h"

#include "AudioTools.h"
#include "AudioTools/Communication/A2DPStream.h"

static I2SStream  i2sIn;
static A2DPStream a2dp;
static StreamCopy copier(a2dp, i2sIn);
static String     btNameStorage;   // muss fuer die Lebensdauer der A2DP-Verbindung gueltig bleiben

void btAudioInit(const String &btDeviceName) {
  btNameStorage = btDeviceName;

  auto cfgA2DP = a2dp.defaultConfig(TX_MODE);
  cfgA2DP.name = btNameStorage.c_str();
  cfgA2DP.auto_reconnect = true;
  a2dp.begin(cfgA2DP);
  a2dp.setVolume(0.8);

  // PSRAM-loses Board (Stolperstein #11): bewusst KEINE zusaetzliche
  // ResampleStream-Stufe hier - der S3 (Master) normalisiert bereits auf
  // eine feste Rate (Stolperstein #10). Falls trotzdem Ruckler auftreten,
  // ist eine ResampleStream zwischen i2sIn und a2dp der naechste Schritt
  // (siehe Stolperstein #14 fuer die Diagnose-Reihenfolge).
  auto i2sCfg = i2sIn.defaultConfig(RX_MODE);
  i2sCfg.pin_bck   = I2S_BCLK;
  i2sCfg.pin_ws    = I2S_LRCK;
  i2sCfg.pin_data  = I2S_DIN;
  i2sCfg.sample_rate     = AUDIO_SAMPLE_RATE;
  i2sCfg.channels        = AUDIO_CHANNELS;
  i2sCfg.bits_per_sample = AUDIO_BITS;
  i2sCfg.is_master       = false;   // S3 liefert den Takt, DevKit ist Slave
  i2sIn.begin(i2sCfg);
}

void btAudioLoop() {
  copier.copy();
}

bool btAudioIsConnected() {
  return a2dp.isConnected();
}
