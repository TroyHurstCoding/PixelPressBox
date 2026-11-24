// sound.cpp - plays short .wav / .mp3 clips from the SD card through the ES8311 codec.
//
// A small player instead of ESP32-audioI2S: that library carried AAC, FLAC, Opus, Vorbis and web-radio
// decoders (about 1 MB of flash) for clips that are only ever WAV or MP3. Output is always 48 kHz
// 16-bit stereo, which is what the codec is clocked for (see es8311.cpp), so any source rate is
// resampled here with linear interpolation.
#include "sound.h"
#include <ESP_I2S.h>
#include <SD.h>
#include <Wire.h>
#include <algorithm>
#include <cmath>
#include "es8311.h"
#include "model.h"
#include "board.h"

#define MINIMP3_ONLY_MP3
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

static const uint32_t OUT_RATE = 48000;
static const int IN_FRAMES = MINIMP3_MAX_SAMPLES_PER_FRAME / 2;  // 1152 stereo frames per block
static const int OUT_FRAMES = IN_FRAMES * OUT_RATE / 8000 + 4;   // enough for an 8 kHz source
static const size_t MP3_BUF = 16 * 1024;

static I2SClass i2s;
static ES8311 codec;
static bool ready = false;
static QueueHandle_t requests = nullptr;     // one pending path; a new request replaces the clip playing
static volatile bool playing = false;
static volatile int32_t gain = 0;            // Q15, from the 0-21 volume

static int16_t *inBuf = nullptr;             // decoded source, interleaved stereo
static int16_t *outBuf = nullptr;            // resampled 48 kHz stereo
static uint8_t *mp3Buf = nullptr;
static mp3dec_t *mp3 = nullptr;

struct Path { char s[96]; };

static bool interrupted() { return uxQueueMessagesWaiting(requests) > 0; }

// ---- resampler: streams stereo blocks at any rate out to the I2S port at 48 kHz
struct Resampler {
  uint32_t step = 1 << 16, pos = 0;
  int16_t prevL = 0, prevR = 0;
  void reset(uint32_t rate) { step = (uint32_t)(((uint64_t)rate << 16) / OUT_RATE); pos = 0; prevL = prevR = 0; }
  // frame index 0 is the last frame of the previous block, 1..n are this block's frames
  void push(const int16_t *in, int n) {
    if (n <= 0) return;
    int32_t g = gain;
    int out = 0;
    while ((pos >> 16) < (uint32_t)n) {
      int i = pos >> 16;
      int32_t f = pos & 0xFFFF;
      int32_t aL = i ? in[2 * (i - 1)] : prevL, aR = i ? in[2 * (i - 1) + 1] : prevR;
      int32_t l = aL + (((in[2 * i] - aL) * f) >> 16);
      int32_t r = aR + (((in[2 * i + 1] - aR) * f) >> 16);
      outBuf[2 * out] = (int16_t)((l * g) >> 15);
      outBuf[2 * out + 1] = (int16_t)((r * g) >> 15);
      out++;
      pos += step;
    }
    pos -= (uint32_t)n << 16;
    prevL = in[2 * (n - 1)];
    prevR = in[2 * (n - 1) + 1];
    i2s.write((const uint8_t *)outBuf, out * 4);  // blocks on the DMA queue, so the task sleeps here
  }
};
static Resampler rs;

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return p[0] | p[1] << 8; }

static void playWav(File &f) {
  uint8_t h[12];
  if (f.read(h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return;
  uint16_t fmt = 0, ch = 0, bits = 0;
  uint32_t rate = 0, left = 0;
  for (;;) {  // walk the chunks to "fmt " and "data"
    uint8_t c[8];
    if (f.read(c, 8) != 8) return;
    uint32_t len = le32(c + 4);
    if (!memcmp(c, "fmt ", 4)) {
      uint8_t b[16];
      if (len < 16 || f.read(b, 16) != 16) return;
      fmt = le16(b), ch = le16(b + 2), rate = le32(b + 4), bits = le16(b + 14);
      f.seek(f.position() + len - 16 + (len & 1));
    } else if (!memcmp(c, "data", 4)) {
      left = len;
      break;
    } else {
      f.seek(f.position() + len + (len & 1));
    }
  }
  if (fmt != 1 || (ch != 1 && ch != 2) || (bits != 8 && bits != 16) || rate < 8000) {
    Serial.printf("[sound] unsupported wav (fmt %u, %u ch, %u bit)\n", fmt, ch, bits);
    return;
  }
  rs.reset(rate);
  const int bytesPerFrame = ch * bits / 8;
  uint8_t *raw = (uint8_t *)outBuf;  // outBuf is free until push(), and bigger than a block of input
  while (left && !interrupted()) {
    int frames = std::min<uint32_t>(IN_FRAMES, left / bytesPerFrame);
    if (!frames) break;
    int got = f.read(raw, frames * bytesPerFrame) / bytesPerFrame;
    if (got <= 0) break;
    left -= got * bytesPerFrame;
    for (int i = 0; i < got; i++) {  // to signed 16-bit stereo
      int16_t l, r;
      if (bits == 16) {
        l = (int16_t)le16(raw + i * bytesPerFrame);
        r = ch == 2 ? (int16_t)le16(raw + i * bytesPerFrame + 2) : l;
      } else {
        l = (int16_t)((raw[i * ch] - 128) << 8);
        r = ch == 2 ? (int16_t)((raw[i * ch + 1] - 128) << 8) : l;
      }
      inBuf[2 * i] = l;
      inBuf[2 * i + 1] = r;
    }
    rs.push(inBuf, got);
  }
}

static void playMp3(File &f) {
  // skip an ID3v2 tag (it can hold cover art far larger than the read buffer)
  uint8_t id3[10];
  if (f.read(id3, 10) == 10 && !memcmp(id3, "ID3", 3))
    f.seek(10 + ((id3[6] & 0x7F) << 21 | (id3[7] & 0x7F) << 14 | (id3[8] & 0x7F) << 7 | (id3[9] & 0x7F)) +
           ((id3[5] & 0x10) ? 10 : 0));
  else
    f.seek(0);
  mp3dec_init(mp3);
  size_t have = 0;
  uint32_t rate = 0;
  bool eof = false;
  while (!interrupted()) {
    if (!eof && have < MP3_BUF) {
      int n = f.read(mp3Buf + have, MP3_BUF - have);
      if (n <= 0) eof = true;
      else have += n;
    }
    if (!have) break;
    mp3dec_frame_info_t info;
    int samples = mp3dec_decode_frame(mp3, mp3Buf, have, inBuf, &info);
    if (!info.frame_bytes) break;  // nothing decodable left
    have -= info.frame_bytes;
    memmove(mp3Buf, mp3Buf + info.frame_bytes, have);
    if (!samples) continue;
    if ((uint32_t)info.hz != rate) rs.reset(rate = info.hz);
    if (info.channels == 1)  // widen mono in place, back to front
      for (int i = samples - 1; i >= 0; i--) inBuf[2 * i] = inBuf[2 * i + 1] = inBuf[i];
    rs.push(inBuf, samples);
  }
}

// ---------------------------------------------------------------- microphones (ES7210, I2C 0x40)
// The ES7210 shares the speaker's I2S clocks, so it is set to plain 16-bit I2S (mics 1 and 2 as left/right)
// instead of the 4-slot TDM of Seengreat's demo. One task reads it and keeps a 16-band spectrum for the
// visualizer and listens for two claps.
static const uint8_t ES7210 = 0x40;
static bool micFound = false;
static volatile bool wantSpectrum = false, wantClaps = false;
static volatile bool doubleClap = false;
static uint8_t bandsOut[16];
static uint32_t lastPlayEnd = 0;

static bool esWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES7210);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool micInit() {
  Wire.beginTransmission(ES7210);
  if (Wire.endTransmission() != 0) return false;
  bool ok = true;
  ok &= esWrite(0x00, 0xFF);  // reset
  delay(20);
  ok &= esWrite(0x00, 0x32);
  delay(20);
  ok &= esWrite(0x09, 0x30);  // power-up timing
  ok &= esWrite(0x0A, 0x30);
  ok &= esWrite(0x23, 0x2A);  // ADC digital paths + high-pass filter
  ok &= esWrite(0x22, 0x0A);
  ok &= esWrite(0x21, 0x2A);
  ok &= esWrite(0x20, 0x0A);
  ok &= esWrite(0x11, 0x60);  // 16-bit, standard I2S
  ok &= esWrite(0x12, 0x00);  // no TDM
  ok &= esWrite(0x40, 0xC3);  // analog power, VMID
  ok &= esWrite(0x41, 0x70);  // mic bias
  ok &= esWrite(0x42, 0x70);
  for (uint8_t r = 0x43; r <= 0x46; r++) ok &= esWrite(r, CFG.micGain);
  for (uint8_t r = 0x47; r <= 0x4A; r++) ok &= esWrite(r, 0x08);  // PGA on
  ok &= esWrite(0x07, 0x20);  // 48 kHz from MCLK = 256 fs
  ok &= esWrite(0x02, 0xC1);
  ok &= esWrite(0x04, 0x01);
  ok &= esWrite(0x05, 0x00);
  ok &= esWrite(0x06, 0x04);
  ok &= esWrite(0x4B, 0x0F);
  ok &= esWrite(0x4C, 0x0F);
  for (uint8_t r = 0x1B; r <= 0x1E; r++) ok &= esWrite(r, 0xBF);  // 0 dB digital volume
  ok &= esWrite(0x00, 0x71);
  delay(20);
  ok &= esWrite(0x00, 0x41);
  return ok;
}

// in-place radix-2 FFT
static void fft(float *re, float *im, int n) {
  for (int i = 1, j = 0; i < n; i++) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(re[i], re[j]), std::swap(im[i], im[j]);
  }
  for (int len = 2; len <= n; len <<= 1) {
    float a = -2 * PI / len, wr = cosf(a), wi = sinf(a);
    for (int i = 0; i < n; i += len) {
      float cr = 1, ci = 0;
      for (int k = 0; k < len / 2; k++) {
        float ur = re[i + k], ui = im[i + k];
        float vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci;
        float vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
        re[i + k] = ur + vr, im[i + k] = ui + vi;
        re[i + k + len / 2] = ur - vr, im[i + k + len / 2] = ui - vi;
        float t = cr * wr - ci * wi;
        ci = cr * wi + ci * wr;
        cr = t;
      }
    }
  }
}

static void micTask(void *) {
  const int N = 512, BLOCK = 480;  // FFT size; 10 ms blocks for the clap detector
  int16_t *raw = (int16_t *)ps_malloc(BLOCK * 2 * sizeof(int16_t));
  float *re = (float *)ps_malloc(N * sizeof(float)), *im = (float *)ps_malloc(N * sizeof(float));
  float *hist = (float *)ps_malloc(N * sizeof(float)), *win = (float *)ps_malloc(N * sizeof(float));
  if (!raw || !re || !im || !hist || !win) vTaskDelete(nullptr);
  for (int i = 0; i < N; i++) win[i] = 0.5f - 0.5f * cosf(2 * PI * i / (N - 1));  // Hann
  memset(hist, 0, N * sizeof(float));
  float floorRms = 200, peakDb = 60, smooth[16] = {0};
  uint32_t onsetAt = 0, firstClap = 0;
  float onsetRms = 0;
  int edges[17];
  for (int i = 0; i <= 16; i++) edges[i] = std::max(2, (int)lroundf(150 * powf(12000.0f / 150, i / 16.0f) / (48000.0f / N)));
  for (;;) {
    size_t got = i2s.readBytes((char *)raw, BLOCK * 4);
    int frames = got / 4;
    if (frames <= 0) {
      vTaskDelay(10);
      continue;
    }
    if (!wantSpectrum && !wantClaps) continue;  // keep draining so the input never backs up
    // mono, level
    float sum2 = 0, peak = 0;
    memmove(hist, hist + frames, (N - frames) * sizeof(float));
    for (int i = 0; i < frames; i++) {
      float v = (raw[2 * i] + raw[2 * i + 1]) * 0.5f;
      hist[N - frames + i] = v;
      sum2 += v * v;
      peak = std::max(peak, fabsf(v));
    }
    float rms = sqrtf(sum2 / frames);
    uint32_t now = millis();
    if (wantClaps) {
      bool quiet = !playing && now - lastPlayEnd > 800;  // our own speaker doesn't count
      if (onsetAt && now - onsetAt >= 60) {  // a clap dies away fast; a voice or music doesn't
        if (rms < onsetRms * 0.35f) {
          if (firstClap && onsetAt - firstClap > 150 && onsetAt - firstClap < 700) doubleClap = true, firstClap = 0;
          else firstClap = onsetAt;
        }
        onsetAt = 0;
      } else if (!onsetAt && quiet && peak > 6000 && rms > floorRms * 6) {
        onsetAt = now, onsetRms = rms;
      }
      if (firstClap && now - firstClap > 800) firstClap = 0;
      if (!onsetAt) floorRms = floorRms * 0.98f + std::min(rms, floorRms * 3) * 0.02f;
    }
    if (wantSpectrum) {
      for (int i = 0; i < N; i++) re[i] = hist[i] * win[i], im[i] = 0;
      fft(re, im, N);
      float top = 0, db[16];
      for (int b = 0; b < 16; b++) {
        float m = 0;
        for (int k = edges[b]; k < std::max(edges[b] + 1, edges[b + 1]); k++) m = std::max(m, re[k] * re[k] + im[k] * im[k]);
        db[b] = 10 * log10f(m + 1);
        top = std::max(top, db[b]);
      }
      peakDb = std::max(top, peakDb * 0.997f + top * 0.003f);  // slow automatic gain
      for (int b = 0; b < 16; b++) {
        float v = std::max(0.0f, std::min(1.0f, (db[b] - (peakDb - 42)) / 42));
        smooth[b] = v > smooth[b] ? v : smooth[b] * 0.85f + v * 0.15f;
        bandsOut[b] = (uint8_t)(smooth[b] * 255);
      }
    }
  }
}

bool micPresent() { return micFound; }
void micUse(bool spectrum, bool claps) {
  wantSpectrum = spectrum;
  wantClaps = claps;
}
int micBands(uint8_t *out, int n) {
  n = std::min(n, 16);
  memcpy(out, bandsOut, n);
  return n;
}
bool micTakeDoubleClap() {
  bool c = doubleClap;
  doubleClap = false;
  return c;
}

static void audioTask(void *) {
  Path p;
  for (;;) {
    xQueueReceive(requests, &p, portMAX_DELAY);  // sleeps until there is something to play
    File f = SD.open(p.s);
    if (!f) {
      Serial.printf("[sound] couldn't open %s\n", p.s);
      playing = uxQueueMessagesWaiting(requests) > 0;
      continue;
    }
    String s = p.s;
    s.toLowerCase();
    if (s.endsWith(".mp3")) playMp3(f);
    else playWav(f);
    f.close();
    lastPlayEnd = millis();
    playing = uxQueueMessagesWaiting(requests) > 0;
  }
}

bool soundBegin() {
  // audio chips' power is switched by the PCA9557 (done in expanderBegin), the amp by AMP_EN
  pinMode(AMP_EN, OUTPUT);
  digitalWrite(AMP_EN, HIGH);
  delay(20);
  if (!codec.begin(I2C_SDA, I2C_SCL, 400000)) {
    Serial.println("[sound] ES8311 codec not found");
    return false;
  }
  codec.setVolume(100);
  codec.setBitsPerSample(16);
  micFound = micInit();  // the codec's begin() started the I2C bus
  Serial.printf("[sound] microphones %s\n", micFound ? "found" : "not found");
  i2s.setPins(I2S_BCLK, I2S_LRCK, I2S_DOUT, micFound ? I2S_DIN : -1, I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, OUT_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("[sound] I2S start failed");
    return false;
  }
  inBuf = (int16_t *)ps_malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * 2 * sizeof(int16_t));
  outBuf = (int16_t *)ps_malloc(OUT_FRAMES * 2 * sizeof(int16_t));
  mp3Buf = (uint8_t *)ps_malloc(MP3_BUF);
  mp3 = (mp3dec_t *)ps_malloc(sizeof(mp3dec_t));
  if (!inBuf || !outBuf || !mp3Buf || !mp3) {
    Serial.println("[sound] out of memory");
    return false;
  }
  soundSetVolume(CFG.volume);
  requests = xQueueCreate(1, sizeof(Path));
  // minimp3 keeps a ~16 KB scratch area on the stack while decoding a frame
  xTaskCreatePinnedToCore(audioTask, "audio", 24 * 1024, nullptr, 2, nullptr, 1);
  if (micFound) xTaskCreatePinnedToCore(micTask, "mic", 4096, nullptr, 1, nullptr, 0);
  ready = true;
  Serial.println("[sound] ready");
  return true;
}

void soundLoop() {}  // playback is serviced by audioTask

bool soundBusy() { return ready && playing; }

void soundSetVolume(uint8_t v) {
  if (v > 21) v = 21;
  gain = (int32_t)v * v * 32767 / (21 * 21);  // square curve, like the old library's default
}

static String findFile(const String &base) {
  for (const char *ext : {".mp3", ".wav"}) {
    String p = "/sounds/" + base + ext;
    if (SD.exists(p)) return p;
  }
  return "";
}

bool soundPlay(const String &name, const String &team) {
  if (!ready || !CFG.soundOn) return false;
  // team-specific sound wins: /sounds/LV_td.mp3, then /sounds/td.mp3
  String path = team.length() ? findFile(team + "_" + name) : "";
  if (!path.length()) path = findFile(name);
  if (!path.length() || path.length() >= sizeof(Path::s)) return false;
  Path p;
  strcpy(p.s, path.c_str());
  playing = true;
  xQueueOverwrite(requests, &p);  // the clip playing stops at its next block and this one starts
  return true;
}
