#include "motion.h"
#include "model.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

void mapAxes(int16_t rx, int16_t ry, int16_t rz, int xSign, int zSign, float &gx, float &gy, float &gz, bool onPanel) {
  // An accelerometer at rest reads the reaction to gravity, so "down" is minus the reading.
  // Panel: sensor Y runs up/down, Z through the screen (chip side facing the back). Fin: Z up/down, Y through.
  applyAxisMap(axisMapFor(onPanel, xSign, zSign), rx, ry, rz, gx, gy, gz);
}

AxisMap axisMapFor(bool onPanel, int xSign, int zSign) {
  AxisMap m;
  m.axis[0] = 0, m.sign[0] = -xSign;
  if (onPanel) m.axis[1] = 1, m.sign[1] = -zSign, m.axis[2] = 2, m.sign[2] = 1;
  else m.axis[1] = 2, m.sign[1] = -zSign, m.axis[2] = 1, m.sign[2] = -1;
  return m;
}

void applyAxisMap(const AxisMap &m, int16_t rx, int16_t ry, int16_t rz, float &gx, float &gy, float &gz) {
  const int16_t r[3] = {rx, ry, rz};
  gx = m.sign[0] * r[m.axis[0]] / 256.0f;
  gy = m.sign[1] * r[m.axis[1]] / 256.0f;
  gz = m.sign[2] * r[m.axis[2]] / 256.0f;
}

static int dominant(const int16_t r[3]) {  // the axis gravity is on, or -1 if the clock isn't square to anything
  int a = abs(r[0]) > abs(r[1]) ? (abs(r[0]) > abs(r[2]) ? 0 : 2) : (abs(r[1]) > abs(r[2]) ? 1 : 2);
  for (int i = 0; i < 3; i++)
    if (i != a && abs(r[i]) > 150) return -1;  // tilted more than ~35 degrees off that axis
  return abs(r[a]) > 180 ? a : -1;
}

bool axisMapFromPoses(const int16_t up[3], const int16_t back[3], AxisMap &out) {
  int a1 = dominant(up), a2 = dominant(back);
  if (a1 < 0 || a2 < 0 || a1 == a2) return false;
  int a0 = 3 - a1 - a2;
  int s1 = up[a1] > 0 ? 1 : -1;      // standing upright: "down" is toward the screen's bottom edge (gy = +1)
  int s2 = back[a2] > 0 ? -1 : 1;    // on its back, screen up: "down" points away from you (gz = -1)
  // the sensor's axes are right-handed and (right, down, toward you) is left-handed: that fixes the last sign
  int perm = ((a0 + 1) % 3 == a1) ? 1 : -1;
  out.axis[0] = a0, out.axis[1] = a1, out.axis[2] = a2;
  out.sign[0] = perm * s1 * s2, out.sign[1] = s1, out.sign[2] = s2;
  return true;
}

// Which way up, from where gravity points across the screen. The clock stays in its current orientation until it
// has been turned more than halfway (45 degrees, plus a few degrees so it can't flicker on the line) toward the
// next one. Lying nearly flat there's no telling, so it keeps what it had.
int classifyOrient(float gx, float gy, float gz, int prev, bool &flat) {
  static const float ANGLE[4] = {0, -90, 180, 90};  // orient 0 landscape, 1 left edge down, 2 upside down, 3 right
  float inPlane = sqrtf(gx * gx + gy * gy);
  flat = fabsf(gz) > 0.9f && inPlane < 0.35f;
  if (inPlane < 0.35f) return prev;
  float ang = atan2f(gx, gy) * 57.29578f;
  auto off = [&](int o) {
    float d = fabsf(ang - ANGLE[o]);
    return d > 180 ? 360 - d : d;
  };
  if (prev >= 0 && prev < 4 && off(prev) <= 51) return prev;
  int best = 0;
  for (int o = 1; o < 4; o++)
    if (off(o) < off(best)) best = o;
  return best;
}

#ifndef PIXELPRESSBOX_SIM
#include <Preferences.h>
#include <Wire.h>

static const uint8_t ADDR = 0x53;
static Motion M;
static int pendingTap = 0;
static int tapCandidate = 0;
static uint32_t tapSince = 0;
static uint32_t doubleTapAt = 0;
static bool doubleTapGuard = false;
static uint32_t motionSince = 0;
static bool pendingShake = false;
static uint32_t spikeAt[6];
static int spikeN = 0;
static uint32_t lastRead = 0, orientSince = 0;
static int candidate = 0;
static int xSign = 1, zSign = 1;  // zSign: sign of whichever sensor axis runs up/down
static bool onPanel = false;
static AxisMap amap;                // sensor axes -> screen axes (from the mount setting, or from calibration)
static float rawS[3] = {0, 0, 0};   // smoothed raw reading, in counts (256 per g)
static int16_t calUp[3];
static bool haveUp = false;

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
  // THRESH_TAP, 62.5 mg per step: firm 3.5 g (the original, needed a hard knock), light 1.9 g (default), very light 1.25 g
  wr(0x1D, CFG.tapForce == "firm" ? 0x38 : CFG.tapForce == "verylight" ? 0x14 : 0x1E);
  wr(0x21, 0x10);   // DUR: tap shorter than 10 ms
  wr(0x22, 0x20);   // LATENT: 40 ms before a second tap can count
  wr(0x23, 0xC0);   // WINDOW: second tap within 240 ms
  wr(0x2A, 0x07);   // TAP_AXES: x, y, z
  wr(0x2E, 0x60);   // INT_ENABLE: single + double tap (we poll INT_SOURCE; no interrupt wire needed)
  wr(0x2D, 0x08);   // POWER_CTL: measure
  delay(20);
  xSign = CFG.accelMirror ? -1 : 1;
  onPanel = CFG.accelMount == "panel";
  // Which way is up. On the panel the sensor is fixed to the display, so the answer never changes: pin row along
  // the lower edge means sensor -Y is "down" (use calibration, or "Turn the picture upside down", if yours differs).
  // It used to be guessed from how the clock stood at power-up, which made the picture depend on how you happened
  // to plug it in. The old fin mounting keeps that guess, since that board could go on either way up.
  Preferences p;
  p.begin("motion", false);
  zSign = 1;
  if (!onPanel) {
    zSign = p.getChar("zsign", 1);
    int16_t x, y, z;
    if (readRaw(x, y, z) && abs(z) > 220 && abs(x) < 100 && abs(y) < 100) {
      int want = (-z > 0) ? 1 : -1;  // landscape upright means gy > 0
      if (want != zSign && !p.isKey("zlock")) {
        zSign = want;
        p.putChar("zsign", zSign);
      }
    }
  }
  amap = axisMapFor(onPanel, xSign, zSign);
  if (p.isKey("cal") && p.getBytesLength("cal") == 6) {  // calibrated from the phone page: that beats the mount guess
    int8_t c[6];
    p.getBytes("cal", c, 6);
    bool ok = true;
    for (int i = 0; i < 3; i++) ok = ok && c[i] >= 0 && c[i] < 3 && (c[i + 3] == 1 || c[i + 3] == -1);
    if (ok && c[0] != c[1] && c[1] != c[2] && c[0] != c[2]) {
      for (int i = 0; i < 3; i++) amap.axis[i] = c[i], amap.sign[i] = c[i + 3];
      M.calibrated = true;
    }
  }
  p.end();
  M.present = true;
  Serial.printf("[motion] ADXL345 ok, %s, axes %d%+d %d%+d %d%+d\n",
                M.calibrated ? "calibrated" : (onPanel ? "mount=panel" : "mount=fin"), amap.axis[0], amap.sign[0],
                amap.axis[1], amap.sign[1], amap.axis[2], amap.sign[2]);
  return true;
}

void motionPoll() {
  if (!M.present || millis() - lastRead < 20) return;
  lastRead = millis();
  int16_t x, y, z;
  if (!readRaw(x, y, z)) return;
  float gx, gy, gz;
  applyAxisMap(amap, x, y, z, gx, gy, gz);
  rawS[0] += 0.25f * (x - rawS[0]);
  rawS[1] += 0.25f * (y - rawS[1]);
  rawS[2] += 0.25f * (z - rawS[2]);
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
  const float dx = gx - M.gx, dy = gy - M.gy, dz = gz - M.gz;
  // Use near-gravity samples to detect a turn without classifying the tap impulse itself as a turn.
  if (fabsf(mag - 1.0f) < 0.2f && dx * dx + dy * dy + dz * dz > 0.0625f)
    motionSince = lastRead;
  const float a = 0.25f;  // smooth out jitter
  M.gx += a * (gx - M.gx);
  M.gy += a * (gy - M.gy);
  M.gz += a * (gz - M.gz);
  bool flat;
  int o = classifyOrient(M.gx, M.gy, M.gz, M.orient, flat);
  if (flat != M.flat || o != M.orient || pendingShake) motionSince = lastRead;
  M.flat = flat;
  if (o != candidate) {
    candidate = o;
    orientSince = millis();
  } else if (o != M.orient && millis() - orientSince > 600) {  // held for 0.6 s: really turned
    M.orient = o;
  }
  static uint32_t lastLog = 0;
  if (millis() - lastLog >= 1000) {  // once a second on the USB log: handy when checking how the sensor is mounted
    lastLog = millis();
    Serial.printf("[motion] raw %+.2f %+.2f %+.2f  screen %+.2f %+.2f %+.2f  orient %d%s\n", rawS[0] / 256, rawS[1] / 256,
                  rawS[2] / 256, M.gx, M.gy, M.gz, M.orient, M.flat ? " flat" : "");
  }
  uint8_t src = 0;
  const bool haveInterrupt = rd(0x30, &src, 1);  // INT_SOURCE (reading clears it)
  if (doubleTapGuard && lastRead - doubleTapAt < 400) return;
  doubleTapGuard = false;
  if (lastRead - motionSince < 300) {
    tapCandidate = 0;
    return;
  }
  if (haveInterrupt && (src & 0x20)) {
    tapCandidate = 2;
    tapSince = lastRead;
  } else if (haveInterrupt && (src & 0x40) && !tapCandidate) {
    tapCandidate = 1;
    tapSince = lastRead;
  }
  // Allow the hardware double-tap window plus one poll before releasing a single.
  if (tapCandidate && lastRead - tapSince >= (tapCandidate == 1 ? 300u : 100u)) {
    pendingTap = tapCandidate;
    if (tapCandidate == 2) {
      doubleTapAt = lastRead;
      doubleTapGuard = true;
    }
    Serial.printf("[motion] %s tap\n", tapCandidate == 2 ? "double" : "single");
    tapCandidate = 0;
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

// ---- calibration from the phone page: two poses pin down how the sensor is mounted, whatever way that is
const char *motionCalibrate(int step) {
  if (!M.present) return "No motion sensor found";
  Preferences p;
  if (step == 0) {
    p.begin("motion", false);
    p.remove("cal");
    p.end();
    amap = axisMapFor(onPanel, xSign, zSign);
    M.calibrated = false;
    haveUp = false;
    return "Calibration cleared";
  }
  int16_t now[3] = {(int16_t)rawS[0], (int16_t)rawS[1], (int16_t)rawS[2]};
  if (step == 1) {
    if (dominant(now) < 0) return "Hold it straight upright and still, then try again";
    memcpy(calUp, now, sizeof(calUp));
    haveUp = true;
    return "Step 1 saved. Now lay it on its back, screen facing up, and press step 2";
  }
  if (!haveUp) return "Do step 1 first";
  AxisMap m;
  if (!axisMapFromPoses(calUp, now, m)) return "Lay it flat on its back and still, then try again";
  int8_t c[6];
  for (int i = 0; i < 3; i++) c[i] = m.axis[i], c[i + 3] = m.sign[i];
  p.begin("motion", false);
  p.putBytes("cal", c, 6);
  p.end();
  amap = m;
  M.calibrated = true;
  haveUp = false;
  return "Calibrated. Stand it back up";
}

void motionRaw(float out[3]) {
  for (int i = 0; i < 3; i++) out[i] = rawS[i] / 256.0f;
}
#else
const char *motionCalibrate(int) { return "No motion sensor found"; }
void motionRaw(float out[3]) { out[0] = out[1] = out[2] = 0; }
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
