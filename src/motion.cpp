#include "motion.h"
#include "model.h"
#include <cmath>

void mapAxes(int16_t rx, int16_t ry, int16_t rz, int xSign, int zSign, float &gx, float &gy, float &gz) {
  // An accelerometer at rest reads the reaction to gravity, so "down" is minus the reading.
  gy = -rz * zSign / 256.0f;
  gx = -rx * xSign / 256.0f;
  gz = -ry / 256.0f;
}

int classifyOrient(float gx, float gy, float gz, int prev, bool &flat) {
  float ax = fabsf(gx), ay = fabsf(gy), az = fabsf(gz);
  flat = az > 0.8f && ax < 0.45f && ay < 0.45f;
  if (flat) return prev;
  if (ay > 0.7f && ax < 0.5f) return gy > 0 ? 0 : 2;
  if (ax > 0.7f && ay < 0.5f) return gx > 0 ? 3 : 1;
  return prev;  // somewhere in between: keep what we had
}

#ifndef PIXELPRESSBOX_SIM
#include <Preferences.h>
#include <Wire.h>

static const uint8_t ADDR = 0x53;
static Motion M;
static int pendingTap = 0;
static bool pendingShake = false;
static uint32_t spikeAt[6];
static int spikeN = 0;
static uint32_t lastRead = 0, orientSince = 0;
static int candidate = 0;
static int xSign = 1, zSign = 1;

static bool wr(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}
static bool rd(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ADDR, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}
static bool readRaw(int16_t &x, int16_t &y, int16_t &z) {
  uint8_t b[6];
  if (!rd(0x32, b, 6)) return false;
  x = (int16_t)(b[0] | (b[1] << 8));
  y = (int16_t)(b[2] | (b[3] << 8));
  z = (int16_t)(b[4] | (b[5] << 8));
  return true;
}

bool motionBegin() {
  uint8_t id = 0;
  if (!rd(0x00, &id, 1) || id != 0xE5) {  // DEVID
    Serial.println("[motion] no ADXL345 at 0x53");
    return false;
  }
  wr(0x2D, 0x00);   // POWER_CTL: standby while configuring
  wr(0x31, 0x0B);   // DATA_FORMAT: full resolution, +/-16 g (256 counts per g)
  wr(0x2C, 0x0A);   // BW_RATE: 100 Hz
  wr(0x1D, 0x38);   // THRESH_TAP: 3.5 g (62.5 mg/LSB) - a firm tap on the case, not a bump of the desk
  wr(0x21, 0x10);   // DUR: tap shorter than 10 ms
  wr(0x22, 0x20);   // LATENT: 40 ms before a second tap can count
  wr(0x23, 0xC0);   // WINDOW: second tap within 240 ms
  wr(0x2A, 0x07);   // TAP_AXES: x, y, z
  wr(0x2E, 0x60);   // INT_ENABLE: single + double tap (we poll INT_SOURCE; no interrupt wire needed)
  wr(0x2D, 0x08);   // POWER_CTL: measure
  delay(20);
  xSign = CFG.accelMirror ? -1 : 1;
  // mounting check: at power-up the clock is normally standing in landscape. If gravity reads the "wrong" way up
  // along Z, the board was mounted flipped; remember that.
  Preferences p;
  p.begin("motion", false);
  zSign = p.getChar("zsign", 1);
  int16_t x, y, z;
  if (readRaw(x, y, z) && abs(z) > 220 && abs(x) < 100 && abs(y) < 100) {
    int want = (-z > 0) ? 1 : -1;  // landscape upright means gy > 0
    if (want != zSign && !p.isKey("zlock")) {
      zSign = want;
      p.putChar("zsign", zSign);
    }
  }
  p.end();
  M.present = true;
  Serial.printf("[motion] ADXL345 ok, zsign=%d xsign=%d\n", zSign, xSign);
  return true;
}

void motionPoll() {
  if (!M.present || millis() - lastRead < 20) return;
  lastRead = millis();
  int16_t x, y, z;
  if (!readRaw(x, y, z)) return;
  float gx, gy, gz;
  mapAxes(x, y, z, xSign, zSign, gx, gy, gz);
  float mag = sqrtf(gx * gx + gy * gy + gz * gz);
  if (fabsf(mag - 1.0f) > 0.9f) {  // shake: 4 hard jolts within 0.8 s (a single tap is too short to count)
    if (!spikeN || millis() - spikeAt[(spikeN - 1) % 6] > 60) spikeAt[spikeN++ % 6] = millis();
    if (spikeN >= 4) {
      uint32_t first = spikeAt[(spikeN - 4) % 6];
      if (millis() - first < 800 && millis() - first > 180) {
        pendingShake = true;
        spikeN = 0;
      }
    }
  }
  const float a = 0.25f;  // smooth out jitter
  M.gx += a * (gx - M.gx);
  M.gy += a * (gy - M.gy);
  M.gz += a * (gz - M.gz);
  bool flat;
  int o = classifyOrient(M.gx, M.gy, M.gz, M.orient, flat);
  M.flat = flat;
  if (o != candidate) {
    candidate = o;
    orientSince = millis();
  } else if (o != M.orient && millis() - orientSince > 600) {  // held for 0.6 s: really turned
    M.orient = o;
  }
  uint8_t src = 0;
  if (rd(0x30, &src, 1)) {  // INT_SOURCE (reading clears it)
    if (src & 0x20) pendingTap = 2;
    else if ((src & 0x40) && !pendingTap) pendingTap = 1;
  }
}

const Motion &motion() { return M; }

int motionTakeTap() {
  int t = pendingTap;
  pendingTap = 0;
  return t;
}
bool motionTakeShake() {
  bool s = pendingShake;
  pendingShake = false;
  return s;
}
#else
bool motionTakeShake() { return false; }
static Motion M;
bool motionBegin() { return false; }
void motionPoll() {}
const Motion &motion() { return M; }
int motionTakeTap() { return 0; }
#endif

bool motionPortrait() {
  if (CFG.orientation == "portrait") return true;
  if (CFG.orientation == "landscape") return false;
  return M.present && (M.orient == 1 || M.orient == 3);
}

int motionLandscapeRotation() {
  if (CFG.orientation == "landscape" || CFG.orientation == "portrait") return 0;
  return (M.present && M.orient == 2) ? 2 : 0;
}
