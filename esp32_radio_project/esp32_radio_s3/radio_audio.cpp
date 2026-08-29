#include "radio_audio.h"
#include "config.h"

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/Communication/AudioHttp.h"

#if AUDIO_DEBUG_SERIAL
  #define ADBG(...) Serial.printf(__VA_ARGS__)
#else
  #define ADBG(...)
#endif

static I2SStream        i2s;
static ResampleStream    resample(i2s);
static MP3DecoderHelix   codec;
static EncodedAudioStream decoder(&resample, &codec);
static URLStream         url;
static StreamCopy        copier(decoder, url);

static bool   playing        = false;
static bool   pipelineActive = false;   // true zwischen radioAudioStart() und radioAudioStop()
static String statusMsg      = "Starte...";
static String currentTitle   = "";
static String currentArtist  = "";
static String lastUrl        = "";
static unsigned long lastDataMs = 0;

// Watchdog statt unsicherer EOF-Erkennung: reconnectet automatisch, wenn
// längere Zeit keine Daten mehr kopiert wurden (z.B. Verbindungsabbruch).
#define RECONNECT_TIMEOUT_MS 8000

// Stolperstein #15: Sonderzeichen (z.B. Kyrillisch) in Stream-Metadaten
// haben in einem Vergleichsprojekt die Anzeige zum Absturz gebracht.
// Auf druckbares ASCII reduzieren statt roh anzuzeigen.
static String sanitizeMetadata(const char *raw, int len) {
  String out;
  if (!raw) return out;
  int n = (len > 0) ? len : (int)strlen(raw);
  out.reserve(n);
  for (int i = 0; i < n; i++) {
    char c = raw[i];
    if (c >= 32 && c < 127) out += c;
    else if (out.length() && out[out.length() - 1] != ' ') out += ' ';
  }
  out.trim();
  return out;
}

static void applyStreamTitle(const String &s) {
  int sep = s.indexOf(" - ");
  if (sep > 0) {
    currentArtist = s.substring(0, sep);
    currentTitle  = s.substring(sep + 3);
  } else {
    currentTitle  = s;
    currentArtist = "";
  }
}

static void onMetadata(MetaDataType type, const char *str, int len) {
  (void)type;
  String clean = sanitizeMetadata(str, len);
  if (clean.length() == 0) return;
  applyStreamTitle(clean);
  ADBG("[Meta] %s\n", clean.c_str());
}

void radioAudioInit() {
  auto i2sCfg = i2s.defaultConfig(TX_MODE);
  i2sCfg.pin_bck   = I2S_BCLK;
  i2sCfg.pin_ws    = I2S_LRCK;
  i2sCfg.pin_data  = I2S_DOUT;
  i2sCfg.sample_rate     = AUDIO_SAMPLE_RATE;
  i2sCfg.channels        = AUDIO_CHANNELS;
  i2sCfg.bits_per_sample = AUDIO_BITS;
  i2sCfg.is_master       = true;   // S3 liefert den I2S-Takt (siehe Architektur)
  i2s.begin(i2sCfg);

  // ResampleStream normalisiert die vom MP3-Decoder gelieferte, je nach
  // Sender unterschiedliche Rate (32/44.1/48kHz) auf eine feste Ziel-Rate,
  // bevor sie aufs I2S geht. Das war der zentrale Fix im Referenzprojekt
  // (Stolperstein #10) gegen die ~200ms-Ruckler.
  auto rCfg = resample.defaultConfig();
  rCfg.to_sample_rate  = AUDIO_SAMPLE_RATE;
  rCfg.channels        = AUDIO_CHANNELS;
  rCfg.bits_per_sample = AUDIO_BITS;
  resample.begin(rCfg);

  // Sobald der Decoder die tatsächliche Quell-Sample-Rate aus dem MP3-Frame
  // erkennt, wird resample automatisch informiert und passt sich an.
  decoder.addNotifyAudioChange(resample);

  url.setMetadataCallback(onMetadata);
}

void radioAudioStart(const String &newUrl) {
  radioAudioStop();

  playing   = false;
  statusMsg = "Verbinde...";
  currentTitle  = "";
  currentArtist = "";
  lastUrl   = newUrl;

  decoder.begin();
  if (url.begin(newUrl.c_str(), "audio/mp3")) {
    playing   = true;
    statusMsg = "Spielt";
    pipelineActive = true;
    lastDataMs = millis();
  } else {
    statusMsg = "Verbindungsfehler";
  }
}

void radioAudioStop() {
  if (!pipelineActive) return;

  // Stolperstein #14: sauberer Übergang statt hartem Cut, damit die
  // Empfängerseite (DevKitV1) nicht aus dem Takt gerät. Kurze definierte
  // Stille direkt auf den I2S-Ausgang schreiben, dann erst die Quelle
  // schliessen.
  static uint8_t silence[512] = {0};
  for (int i = 0; i < 4; i++) i2s.write(silence, sizeof(silence));

  url.end();
  decoder.end();
  pipelineActive = false;
  playing = false;
}

void radioAudioLoop() {
  if (!pipelineActive) return;
  // copy() liefert bei den meisten AudioTools-Versionen die Anzahl
  // kopierter Bytes zurück (0 = aktuell keine Daten). Falls die
  // installierte Version einen anderen Rückgabetyp hat, hier gegen die
  // tatsächliche Signatur prüfen.
  size_t n = copier.copy();
  if (n > 0) lastDataMs = millis();

  if (playing && millis() - lastDataMs > RECONNECT_TIMEOUT_MS) {
    statusMsg = "Reconnect...";
    String url2 = lastUrl;
    radioAudioStart(url2);
  }
}

bool   radioAudioIsPlaying()   { return playing; }
String radioAudioStatusMsg()   { return statusMsg; }
String radioAudioCurrentTitle(){ return currentTitle; }
String radioAudioCurrentArtist(){ return currentArtist; }
