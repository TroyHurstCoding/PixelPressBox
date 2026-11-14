#include "render2.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include "astro.h"
#include "desk.h"
#include "tronbyt_assets.h"  // BIG_DIGITS for the tall clock
#include "world_map.h"

// helpers exported by render.cpp
MatrixPanel_I2S_DMA *gfxPanel();
void gfxBegin();
void gfxEnd();
uint16_t gfxColor(uint32_t c);
void gfxText(const String &s, int x, int y, uint32_t color, int size);

static inline void px(int x, int y, uint32_t c) { gfxPanel()->drawPixel(x, y, gfxColor(c)); }
static inline void fill(int x, int y, int w, int h, uint32_t c) { gfxPanel()->fillRect(x, y, w, h, gfxColor(c)); }
static inline void txt(const String &s, int x, int y, uint32_t c, int size = 1) { gfxText(s, x, y, c, size); }

static uint32_t rnd32() {
  static uint32_t s = 2463534242u;
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}
static float frand() { return (rnd32() & 0xFFFFFF) / 16777216.0f; }
static uint32_t scale(uint32_t c, float k) {
  k = std::max(0.0f, std::min(1.0f, k));
  return ((uint32_t)(((c >> 16) & 255) * k) << 16) | ((uint32_t)(((c >> 8) & 255) * k) << 8) | (uint32_t)((c & 255) * k);
}
static uint32_t mix(uint32_t a, uint32_t b, float t) {
  t = std::max(0.0f, std::min(1.0f, t));
  auto ch = [&](int sh) { return (uint32_t)(((a >> sh) & 255) * (1 - t) + ((b >> sh) & 255) * t) << sh; };
  return ch(16) | ch(8) | ch(0);
}
static uint32_t hsv(float h, float s, float v) {
  h = fmodf(h, 1.0f) * 6;
  int i = (int)h;
  float f = h - i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
  float r, g, b;
  switch (i % 6) {
    case 0: r = v, g = t, b = p; break;
    case 1: r = q, g = v, b = p; break;
    case 2: r = p, g = v, b = t; break;
    case 3: r = p, g = q, b = v; break;
    case 4: r = t, g = p, b = v; break;
    default: r = v, g = p, b = q; break;
  }
  return ((uint32_t)(r * 255) << 16) | ((uint32_t)(g * 255) << 8) | (uint32_t)(b * 255);
}
static void line(int x0, int y0, int x1, int y1, uint32_t c) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for (int n = 0; n < 200; n++) {
    px(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) err += dy, x0 += sx;
    if (e2 <= dx) err += dx, y0 += sy;
  }
}
static void disc(float cx, float cy, float r, uint32_t c, bool shade) {
  for (int y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++)
    for (int x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d2 = dx * dx + dy * dy;
      if (d2 > r * r) continue;
      if (!shade) {
        px(x, y, c);
        continue;
      }
      float lite = 1.0f - 0.55f * sqrtf(d2) / r + 0.25f * (-(dx + dy) / (r * 1.4f));  // lit from the top-left
      px(x, y, scale(c, lite));
    }
}

// =============================================================== aquarium
struct Fish {
  float x, y, vx, phase;
  int type;
};
static const char *const FISH_SPR[4][7] = {
    {"..OWO...", "O.OWOOW.", "OOOWOOWk", "O.OWOOW.", "..OWO...", nullptr, nullptr},   // clownfish
    {"..BBBB..", "Y.BBBBB.", "YYBBBBkB", "Y.BBBBB.", "..BBBB..", nullptr, nullptr},   // blue tang
    {"..CC.", "RRCCk", "..CC.", nullptr, nullptr, nullptr, nullptr},                  // neon tetra
    {"..Y...", ".YKY..", "YYKYYk", ".YKY..", "..Y...", nullptr, nullptr},             // angelfish
};
static uint32_t fishColor(char ch) {
  switch (ch) {
    case 'O': return 0xFF7A00;
    case 'W': return 0xF0F0F0;
    case 'k': return 0x000000;
    case 'B': return 0x2060FF;
    case 'Y': return 0xFFD000;
    case 'C': return 0x30E0FF;
    case 'R': return 0xFF3030;
    case 'K': return 0x302010;
  }
  return 0;
}
static Fish fishes[6];
static bool fishInit = false;
struct Bubble {
  float x, y;
};
static Bubble bubbles[10];

void drawAquarium(int frame) {
  if (!fishInit) {
    for (int i = 0; i < 6; i++) {
      Fish &f = fishes[i];
      f.type = i % 4;
      f.x = frand() * 56;
      f.y = 4 + frand() * 18;
      f.vx = (0.15f + frand() * 0.25f) * (rnd32() & 1 ? 1 : -1);
      f.phase = frand() * 6.28f;
    }
    for (auto &b : bubbles) b = {52.0f + frand() * 2, frand() * 28};
    fishInit = true;
  }
  gfxBegin();
  for (int y = 0; y < 28; y++) fill(0, y, 64, 1, mix(0x00386E, 0x001024, y / 28.0f));
  for (int k = 0; k < 3; k++) {  // slow light rays
    int x0 = (int)(fmodf(frame * 0.05f + k * 23, 80.0f)) - 8;
    for (int y = 0; y < 24; y++) {
      int x = x0 + y / 3;
      if (x >= 0 && x < 64) px(x, y, mix(0x00386E, 0x2A6FA8, 0.35f * (1 - y / 24.0f)));
    }
  }
  for (int y = 28; y < 32; y++)
    for (int x = 0; x < 64; x++) px(x, y, ((x * 7 + y * 13) % 5) ? 0xC8A060 : 0xA08048);
  // rocks
  disc(9, 28, 3.5f, 0x707070, true);
  disc(14, 29, 2.5f, 0x5A5A5A, true);
  // seaweed
  static const int SW_X[4] = {4, 21, 40, 60}, SW_H[4] = {14, 10, 16, 12};
  for (int s = 0; s < 4; s++)
    for (int i = 0; i < SW_H[s]; i++) {
      int y = 28 - i;
      float sway = sinf(frame * 0.06f + s * 1.7f + i * 0.35f) * (i / (float)SW_H[s]) * 1.8f;
      int x = SW_X[s] + (int)lroundf(sway);
      px(x, y, (i % 3) ? 0x1E9E3E : 0x2EC85A);
      if (i % 4 == 2) px(x + ((s & 1) ? 1 : -1), y, 0x178030);
    }
  // bubbles from the bubbler at the right
  px(52, 28, 0x909090);
  px(53, 28, 0x909090);
  for (auto &b : bubbles) {
    b.y -= 0.35f;
    if (b.y < 0) b = {52.0f + frand() * 2, 27.0f};
    int x = (int)(b.x + sinf(b.y * 0.6f) * 0.8f);
    px(x, (int)b.y, 0x9FD8FF);
  }
  // fish
  for (auto &f : fishes) {
    f.x += f.vx;
    if (f.x < -2 || f.x > 60 || (rnd32() % 900) == 0) f.vx = -f.vx;
    if (f.x < -2) f.x = -2;
    if (f.x > 60) f.x = 60;
    float yy = f.y + sinf(frame * 0.07f + f.phase) * 1.2f;
    const char *const *spr = FISH_SPR[f.type];
    int h = 0, w = strlen(spr[0]);
    while (h < 7 && spr[h]) h++;
    bool left = f.vx < 0;
    for (int r = 0; r < h; r++)
      for (int c = 0; c < w; c++) {
        char ch = spr[r][c];
        if (ch == '.') continue;
        int x = (int)f.x + (left ? (w - 1 - c) : c);
        px(x, (int)yy + r, fishColor(ch));
      }
  }
  gfxEnd();
}

// =============================================================== double pendulum
static double dpT1, dpT2, dpW1, dpW2;
struct TrailPt {
  int8_t x, y;
};
static TrailPt dpTrail[90];
static int dpTrailN = 0, dpTrailHead = 0;
static int dpSince = 0;

static void dpDeriv(double t1, double t2, double w1, double w2, double &a1, double &a2) {
  const double g = 9.81, m1 = 1, m2 = 1, L1 = 1, L2 = 1;
  double d = t1 - t2, den = 2 * m1 + m2 - m2 * cos(2 * t1 - 2 * t2);
  a1 = (-g * (2 * m1 + m2) * sin(t1) - m2 * g * sin(t1 - 2 * t2) - 2 * sin(d) * m2 * (w2 * w2 * L2 + w1 * w1 * L1 * cos(d))) /
       (L1 * den);
  a2 = (2 * sin(d) * (w1 * w1 * L1 * (m1 + m2) + g * (m1 + m2) * cos(t1) + w2 * w2 * L2 * m2 * cos(d))) / (L2 * den);
}

void drawDoublePendulum(int frame, bool restart) {
  if (restart || dpSince > 30 * 25) {
    dpT1 = (100 + frand() * 70) * M_PI / 180 * (rnd32() & 1 ? 1 : -1);
    dpT2 = (100 + frand() * 70) * M_PI / 180;
    dpW1 = dpW2 = 0;
    dpTrailN = dpTrailHead = 0;
    dpSince = 0;
  }
  dpSince++;
  const double dt = 1.0 / 30 / 4;
  for (int k = 0; k < 4; k++) {  // RK4
    double a1, a2, b1, b2, c1, c2, e1, e2;
    dpDeriv(dpT1, dpT2, dpW1, dpW2, a1, a2);
    dpDeriv(dpT1 + dpW1 * dt / 2, dpT2 + dpW2 * dt / 2, dpW1 + a1 * dt / 2, dpW2 + a2 * dt / 2, b1, b2);
    double w1b = dpW1 + a1 * dt / 2, w2b = dpW2 + a2 * dt / 2;
    dpDeriv(dpT1 + w1b * dt / 2, dpT2 + w2b * dt / 2, dpW1 + b1 * dt / 2, dpW2 + b2 * dt / 2, c1, c2);
    double w1c = dpW1 + b1 * dt / 2, w2c = dpW2 + b2 * dt / 2;
    dpDeriv(dpT1 + w1c * dt, dpT2 + w2c * dt, dpW1 + c1 * dt, dpW2 + c2 * dt, e1, e2);
    double w1d = dpW1 + c1 * dt, w2d = dpW2 + c2 * dt;
    dpT1 += dt / 6 * (dpW1 + 2 * w1b + 2 * w1c + w1d);
    dpT2 += dt / 6 * (dpW2 + 2 * w2b + 2 * w2c + w2d);
    dpW1 += dt / 6 * (a1 + 2 * b1 + 2 * c1 + e1);
    dpW2 += dt / 6 * (a2 + 2 * b2 + 2 * c2 + e2);
  }
  const float PX = 32, PY = 15, L1 = 8, L2 = 7;
  float x1 = PX + L1 * sinf(dpT1), y1 = PY + L1 * cosf(dpT1);
  float x2 = x1 + L2 * sinf(dpT2), y2 = y1 + L2 * cosf(dpT2);
  dpTrail[dpTrailHead] = {(int8_t)lroundf(x2), (int8_t)lroundf(y2)};
  dpTrailHead = (dpTrailHead + 1) % 90;
  if (dpTrailN < 90) dpTrailN++;
  gfxBegin();
  for (int i = 0; i < dpTrailN; i++) {  // oldest first, fading in, hue drifting
    int idx = (dpTrailHead - dpTrailN + i + 90) % 90;
    float age = (float)i / dpTrailN;
    px(dpTrail[idx].x, dpTrail[idx].y, hsv(frame * 0.004f + age * 0.3f, 0.8f, 0.15f + 0.75f * age));
  }
  line((int)PX, (int)PY, (int)lroundf(x1), (int)lroundf(y1), 0x9090A0);
  line((int)lroundf(x1), (int)lroundf(y1), (int)lroundf(x2), (int)lroundf(y2), 0x9090A0);
  disc(PX, PY, 1.2f, 0x606060, false);
  disc(x1, y1, 1.8f, 0xFFB030, true);
  disc(x2, y2, 1.8f, 0x30C0FF, true);
  gfxEnd();
}

// =============================================================== Newton's cradle
static const int NB = 5;
static float crTh[NB], crW[NB];
static int crIdle = 0, crLift = 0;
static const float CR_L = 19, CR_PY = 3, CR_SP = 7;
static float crPivX(int i) { return 32 + (i - 2) * CR_SP; }

static void cradleLift(int k, bool right) {
  for (int i = 0; i < NB; i++) crTh[i] = crW[i] = 0;
  for (int j = 0; j < k; j++) crTh[right ? NB - 1 - j : j] = right ? 0.95f : -0.95f;
}

void drawCradle(int frame, float gx, float gy, bool accel, bool kick, bool restart) {
  if (restart) {
    cradleLift(1, false);
    crIdle = 0;
    crLift = 0;
  }
  float g = accel ? sqrtf(gx * gx + gy * gy) : 1.0f;
  float phi = accel ? atan2f(gx, std::max(0.2f, gy)) : 0.0f;
  phi = std::max(-1.0f, std::min(1.0f, phi));
  const float K = 23.0f * std::max(0.3f, g);  // (2 pi / period)^2, period ~1.3 s
  if (kick) {  // a tap knocks the end ball outward
    bool right = (frame / 7) & 1;
    crW[right ? NB - 1 : 0] += right ? 3.2f : -3.2f;
  }
  const int SUB = 10;
  const float dt = 1.0f / 30 / SUB;
  for (int s = 0; s < SUB; s++) {
    for (int i = 0; i < NB; i++) {
      crW[i] += (-K * sinf(crTh[i] - phi) - 0.04f * crW[i]) * dt;
      crTh[i] += crW[i] * dt;
    }
    for (int pass = 0; pass < NB; pass++) {  // contacts: equal masses swap velocities
      for (int i = 0; i + 1 < NB; i++) {
        float xa = crPivX(i) + CR_L * sinf(crTh[i]), xb = crPivX(i + 1) + CR_L * sinf(crTh[i + 1]);
        if (xb - xa < CR_SP - 0.001f) {
          if (crW[i] * cosf(crTh[i]) > crW[i + 1] * cosf(crTh[i + 1])) std::swap(crW[i], crW[i + 1]);
          float over = (CR_SP - (xb - xa)) / 2 / CR_L;  // push apart
          crTh[i] -= over;
          crTh[i + 1] += over;
        }
      }
    }
  }
  float energy = 0;
  for (int i = 0; i < NB; i++) energy += crW[i] * crW[i] + K * (1 - cosf(crTh[i] - phi)) * 2;
  if (energy < 0.6f) crIdle++;
  else crIdle = 0;
  if (!accel && crIdle > 30) {  // demo: lift 1, 2, 3, 2 balls in turn
    static const int SEQ[4] = {1, 2, 3, 2};
    cradleLift(SEQ[crLift % 4], crLift & 1);
    crLift++;
    crIdle = 0;
  }
  if (accel && crIdle > 30 * 20) {  // left alone for 20 s: start it once by itself
    cradleLift(1, false);
    crIdle = 0;
  }
  gfxBegin();
  fill(10, 1, 44, 2, 0x8890A0);     // top bar
  fill(10, 1, 1, 30, 0x606878);     // posts
  fill(53, 1, 1, 30, 0x606878);
  fill(8, 30, 48, 2, 0x404858);     // base
  for (int i = 0; i < NB; i++) {
    float bx = crPivX(i) + CR_L * sinf(crTh[i]), by = CR_PY + CR_L * cosf(crTh[i]);
    line((int)crPivX(i), (int)CR_PY, (int)lroundf(bx), (int)lroundf(by), 0x505868);
    disc(bx, by, 3.2f, 0xD8E0EC, true);
    px((int)lroundf(bx) - 1, (int)lroundf(by) - 2, 0xFFFFFF);
  }
  if (accel) px(63, 0, 0x206020);   // tiny dot: the accelerometer is driving it
  gfxEnd();
}

// =============================================================== water (falling-liquid cellular automaton)
static uint8_t wGrid[32][64];
static bool wInit = false;

void drawWater(int frame, float gx, float gy, bool accel, bool restart) {
  if (restart || !wInit) {
    memset(wGrid, 0, sizeof wGrid);
    for (int y = 17; y < 32; y++)
      for (int x = 0; x < 64; x++) wGrid[y][x] = 1;
    wInit = true;
  }
  if (!accel) {  // demo: gently rock the "gravity" back and forth so it sloshes
    float a = 0.75f * sinf(frame * 0.025f) + 0.25f * sinf(frame * 0.061f);
    gx = sinf(a);
    gy = cosf(a);
  }
  float ang = atan2f(gy, gx);  // direction "down" points, in screen terms
  int oct = ((int)lroundf(ang / (float)(M_PI / 4)) + 8) % 8;
  static const int DX[8] = {1, 1, 0, -1, -1, -1, 0, 1}, DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  int dx = DX[oct], dy = DY[oct];
  int lx = DX[(oct + 1) % 8], ly = DY[(oct + 1) % 8], rx = DX[(oct + 7) % 8], ry = DY[(oct + 7) % 8];
  int sx = DX[(oct + 2) % 8], sy = DY[(oct + 2) % 8];  // sideways
  auto free_ = [&](int x, int y) { return x >= 0 && x < 64 && y >= 0 && y < 32 && wGrid[y][x] == 0; };
  for (int pass = 0; pass < 2; pass++) {
    static uint8_t moved[32][64];
    memset(moved, 0, sizeof moved);
    // sweep from the "bottom" (the side gravity points to) toward the top
    int ys = dy >= 0 ? 31 : 0, ye = dy >= 0 ? -1 : 32, yi = dy >= 0 ? -1 : 1;
    int xs = dx >= 0 ? 63 : 0, xe = dx >= 0 ? -1 : 64, xi = dx >= 0 ? -1 : 1;
    for (int y = ys; y != ye; y += yi)
      for (int x = xs; x != xe; x += xi) {
        if (!wGrid[y][x] || moved[y][x]) continue;
        int nx = x, ny = y;
        if (free_(x + dx, y + dy)) nx = x + dx, ny = y + dy;
        else {
          bool lf = free_(x + lx, y + ly), rf = free_(x + rx, y + ry);
          if (lf && rf) (rnd32() & 1) ? (nx = x + lx, ny = y + ly) : (nx = x + rx, ny = y + ry);
          else if (lf) nx = x + lx, ny = y + ly;
          else if (rf) nx = x + rx, ny = y + ry;
          else {
            int s = (rnd32() & 1) ? 1 : -1;
            if (free_(x + s * sx, y + s * sy)) nx = x + s * sx, ny = y + s * sy;
            else if (free_(x - s * sx, y - s * sy)) nx = x - s * sx, ny = y - s * sy;
          }
        }
        if (nx != x || ny != y) {
          wGrid[y][x] = 0;
          wGrid[ny][nx] = 1;
          moved[ny][nx] = 1;
        }
      }
  }
  gfxBegin();
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 64; x++) {
      if (!wGrid[y][x]) continue;
      int ux = x - dx, uy = y - dy;  // the cell "above" against gravity
      bool surface = !(ux >= 0 && ux < 64 && uy >= 0 && uy < 32) ? false : !wGrid[uy][ux];
      int u2x = x - 2 * dx, u2y = y - 2 * dy;
      bool near = !surface && u2x >= 0 && u2x < 64 && u2y >= 0 && u2y < 32 && !wGrid[u2y][u2x];
      float sh = 0.85f + 0.15f * sinf(x * 0.45f + y * 0.3f + frame * 0.18f);
      uint32_t c = surface ? 0x9FE8FF : near ? 0x2A9BEA : scale(0x0E5CC0, sh);
      px(x, y, c);
    }
  gfxEnd();
}

// =============================================================== portrait: beer glass (32 x 64)
static float beerLevel = 0, beerFoam = 0;
static int beerPhase = 0, beerT = 0;  // 0 pour, 1 settle, 2 sip, 3 empty pause
struct BeerBubble {
  float x, y;
};
static BeerBubble beerB[14];
static float glassL(int y) { return 4 + (y - 4) * (4.0f / 56); }
static float glassR(int y) { return 27 - (y - 4) * (4.0f / 56); }

void drawBeer(int frame, float gx, float gy, bool accel, bool tap, bool restart) {
  if (restart) {
    beerLevel = 0;
    beerFoam = 0;
    beerPhase = 0;
    beerT = 0;
    for (auto &b : beerB) b = {10 + frand() * 12, 60 - frand() * 40};
  }
  if (tap) {  // tap the glass: pour another one
    beerPhase = 0;
    beerT = 0;
  }
  beerT++;
  const float FULL = 0.86f;
  switch (beerPhase) {
    case 0:  // pour
      beerLevel = std::min(FULL, beerLevel + 0.006f);
      beerFoam = std::min(6.0f, beerFoam + 0.06f);
      if (beerLevel >= FULL) beerPhase = 1, beerT = 0;
      break;
    case 1:  // settle: the head shrinks a little
      beerFoam = std::max(3.5f, beerFoam - 0.02f);
      if (beerT > 30 * 6) beerPhase = 2, beerT = 0;
      break;
    case 2:  // a sip every few seconds
      if (beerT % 120 < 25) beerLevel = std::max(0.0f, beerLevel - 0.0016f);
      beerFoam = std::max(1.5f, beerFoam - 0.002f);
      if (beerLevel <= 0.02f) beerPhase = 3, beerT = 0;
      break;
    default:
      beerFoam = 0;
      if (beerT > 30 * 3) beerPhase = 0, beerT = 0;
  }
  if (!accel) gx = 0.08f * sinf(frame * 0.03f), gy = 1;
  float slope = -gx / std::max(0.25f, gy);
  slope = std::max(-0.7f, std::min(0.7f, slope));
  float yTop = 60 - beerLevel * 52;
  gfxBegin();
  for (int y = 4; y <= 60; y++) {
    int l = (int)lroundf(glassL(y)), r = (int)lroundf(glassR(y));
    for (int x = l + 1; x < r; x++) {
      float ys = yTop + slope * (x - 16);
      if (beerLevel <= 0.01f) continue;
      if (y >= ys) {
        float depth = (y - ys) / 50.0f;
        uint32_t c = mix(0xF2B230, 0xB45A08, depth);
        if (((x * 5 + y * 3 + frame / 4) % 23) == 0) c = mix(c, 0xFFE080, 0.5f);
        px(x, y, c);
      } else if (y >= ys - beerFoam) {
        px(x, y, ((x + y) % 4 == 0) ? 0xE8DCC0 : 0xFFF6E0);
      }
    }
    px(l, y, 0x7C8C9C);  // the glass
    px(r, y, 0x7C8C9C);
  }
  fill(8, 61, 17, 2, 0x7C8C9C);
  fill(4, 3, 1, 1, 0xB0C0D0);
  fill(27, 3, 1, 1, 0xB0C0D0);
  for (int y = 10; y < 50; y += 3) px((int)glassL(y) + 2, y, 0x506070);  // glint
  // bubbles rise to the surface
  for (auto &b : beerB) {
    b.y -= 0.5f + frand() * 0.3f;
    float ys = yTop + slope * (b.x - 16);
    if (b.y < ys + 1 || beerLevel < 0.05f) b = {10 + frand() * 12, 59};
    else px((int)b.x, (int)b.y, 0xFFE9A0);
  }
  if (beerPhase == 0) {  // the pour, from the tap above
    for (int y = 0; y < yTop - beerFoam; y++) px(16 + ((y + frame) % 5 == 0), y, 0xF0B030);
    fill(13, 0, 7, 1, 0xA0A8B0);
  }
  gfxEnd();
}

// =============================================================== portrait: tall clock
static void bigDigit(int x0, int y0, int d, uint32_t col) {
  uint16_t c = gfxColor(col);
  for (int y = 0; y < 32; y++) {
    uint16_t bits = BIG_DIGITS[d * 32 + y];
    for (int x = 0; x < 13; x++)
      if (bits & (1 << (12 - x))) gfxPanel()->drawPixel(x0 + x, y0 + y, c);
  }
}

void drawPortraitClock(time_t now) {
  struct tm t;
  localtime_r(&now, &t);
  int h = CFG.twelveHour ? (t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12) : t.tm_hour;
  gfxBegin();
  if (h >= 10 || !CFG.twelveHour) bigDigit(2, 0, h / 10, 0xFFFFFF);
  bigDigit(17, 0, h % 10, 0xFFFFFF);
  bigDigit(2, 32, t.tm_min / 10, 0xFFB020);
  bigDigit(17, 32, t.tm_min % 10, 0xFFB020);
  gfxEnd();
}

// =============================================================== portrait: hourglass
static uint8_t hgGrid[64][32];  // 0 empty, 1..3 sand shades
static int hgDone = 0, hgSign = 1, hgNeckTick = 0;
static float hgHalfW(int y) {  // half-width of the glass at row y (0 outside)
  if (y < 4 || y > 59) return 0;
  if (y <= 30) return 2 + (30 - y) * (10.0f / 26);
  if (y <= 32) return 1;
  return 2 + (y - 33) * (10.0f / 26);
}
static bool hgInside(int x, int y) {
  float w = hgHalfW(y);
  return w > 0 && fabsf(x + 0.5f - 16) <= w;
}

void drawHourglass(int frame, float gy, bool accel, bool restart) {
  if (restart) {
    memset(hgGrid, 0, sizeof hgGrid);
    for (int y = 10; y <= 29; y++)
      for (int x = 0; x < 32; x++)
        if (hgInside(x, y)) hgGrid[y][x] = 1 + rnd32() % 3;
    hgSign = 1;
    hgDone = 0;
  }
  int s = accel ? (gy >= 0 ? 1 : -1) : hgSign;
  hgNeckTick++;
  // one sweep, from the downhill end
  int y0 = s > 0 ? 62 : 1, y1 = s > 0 ? 0 : 63;
  bool movedAny = false;
  for (int y = y0; y != y1; y -= s) {
    int dir = (rnd32() & 1) ? 1 : -1;
    for (int k = 0; k < 32; k++) {
      int x = dir > 0 ? k : 31 - k;
      if (!hgGrid[y][x]) continue;
      int ny = y + s;
      if (ny < 0 || ny > 63) continue;
      bool neck = (y == 31 || y == 32);
      if (neck && hgNeckTick % 3) continue;  // the neck lets one grain through every few frames
      int nx = x;
      if (!(hgInside(x, ny) && !hgGrid[ny][x])) {
        int a = x + dir, b = x - dir;
        if (hgInside(a, ny) && !hgGrid[ny][a]) nx = a;
        else if (hgInside(b, ny) && !hgGrid[ny][b]) nx = b;
        else continue;
      }
      hgGrid[ny][nx] = hgGrid[y][x];
      hgGrid[y][x] = 0;
      movedAny = true;
    }
  }
  if (!accel) {  // demo: when it has run out, wait a moment and turn it over
    hgDone = movedAny ? 0 : hgDone + 1;
    if (hgDone > 60) {
      for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) std::swap(hgGrid[y][x], hgGrid[63 - y][x]);
      hgDone = 0;
    }
  }
  gfxBegin();
  for (int y = 4; y <= 59; y++) {
    float w = hgHalfW(y);
    int l = (int)floorf(16 - w) - 1, r = (int)ceilf(16 + w);
    px(l, y, 0x506878);
    px(r, y, 0x506878);
    for (int x = l + 1; x < r; x++)
      if (hgGrid[y][x]) px(x, y, hgGrid[y][x] == 1 ? 0xE8C070 : hgGrid[y][x] == 2 ? 0xD8AE58 : 0xF4D490);
  }
  fill(3, 1, 26, 3, 0x8B5A2B);  // wooden caps and posts
  fill(3, 60, 26, 3, 0x8B5A2B);
  fill(3, 4, 1, 56, 0x6B4220);
  fill(28, 4, 1, 56, 0x6B4220);
  gfxEnd();
}

// =============================================================== portrait: lava lamp
struct Blob {
  float x, y, vy, r, heat;
};
static Blob blobs[6];
static float lampHalfW(int y) {
  if (y < 8 || y > 54) return 0;
  return 6 + (y - 8) * (4.0f / 46);  // narrow at the top, wider at the bottom
}

void drawLava(int frame, bool restart) {
  if (restart || blobs[0].r == 0)
    for (int i = 0; i < 6; i++)
      blobs[i] = {11 + frand() * 10, 12 + i * 7.0f, 0, 1.6f + frand() * 1.4f, (i % 2) ? 0.9f : 0.1f};
  for (int i = 0; i < 6; i++) {
    Blob &b = blobs[i];
    float rate = 0.006f + 0.003f * (i % 3);  // each blob heats and cools at its own pace
    float bottomHeat = (b.y > 47) ? rate : 0, topCool = (b.y < 18) ? rate : 0;
    b.heat = std::max(0.0f, std::min(1.0f, b.heat + bottomHeat - topCool));
    b.vy += (0.5f - b.heat) * 0.004f;  // hot rises (negative y), cool sinks
    b.vy *= 0.96f;
    b.y += b.vy;
    b.x += sinf(frame * 0.015f + i * 1.3f) * 0.06f;
    b.y = std::max(10.0f, std::min(52.0f, b.y));
    b.x = std::max(11.0f, std::min(21.0f, b.x));
  }
  gfxBegin();
  for (int y = 8; y <= 54; y++) {
    float w = lampHalfW(y);
    for (int x = 0; x < 32; x++) {
      float dx0 = x + 0.5f - 16;
      if (fabsf(dx0) > w) continue;
      float f = 0;
      for (auto &b : blobs) {
        float dx = x + 0.5f - b.x, dy = y + 0.5f - b.y;
        f += b.r * b.r / (dx * dx + dy * dy + 0.5f);
      }
      uint32_t c;
      if (f > 1.0f) c = mix(0xFF3070, 0xFF9030, std::min(1.0f, (f - 1.0f) * 0.6f));
      else c = mix(0x1A0830, 0x3A1060, f);
      if (fabsf(dx0) > w - 1) c = scale(c, 0.6f);
      px(x, y, c);
    }
  }
  for (int y = 1; y < 8; y++) fill(16 - (5 + y / 3), y, 2 * (5 + y / 3), 1, 0x9AA4B0);   // cap
  for (int y = 55; y < 64; y++) fill(16 - (10 + (y - 55) / 3), y, 2 * (10 + (y - 55) / 3), 1, 0x8A94A0);  // base
  gfxEnd();
}

// =============================================================== Fly the W
static const char *const W_GLYPH[10] = {
    "##.........##", "##.........##", ".##.......##.", ".##...#...##.", ".##..###..##.",
    "..##.#.#.##..", "..####.####..", "..###...###..", "...##...##...", "...#.....#..."};

void drawFlyW(const GameResult &r, uint32_t teamCol, int frame) {
  gfxBegin();
  for (int y = 0; y < 32; y++) fill(0, y, 64, 1, mix(0x0A1830, 0x163A70, y / 32.0f));
  fill(4, 1, 2, 31, 0xB0B0B0);  // pole
  px(4, 0, 0xFFD700);
  px(5, 0, 0xFFD700);
  const int FW = 30, FH = 18, X0 = 6, Y0 = 2;
  for (int c = 0; c < FW; c++) {
    float wave = sinf(frame * 0.22f - c * 0.32f) * 1.6f * (c / (float)FW);
    float slope = cosf(frame * 0.22f - c * 0.32f);
    int yo = (int)lroundf(wave);
    for (int y = 0; y < FH; y++) {
      uint32_t col = mix(0xFFFFFF, 0xA8B0C0, 0.25f + 0.25f * slope);
      int gx = c - 9, gy = y - 4;
      if (gx >= 0 && gx < 13 && gy >= 0 && gy < 10 && W_GLYPH[gy][gx] == '#') col = scale(teamCol, 0.85f + 0.15f * slope);
      px(X0 + c, Y0 + y + yo, col);
    }
  }
  String a = r.us, b = r.them;
  txt(a, 64 - (int)a.length() * 6, 3, mix(teamCol, 0xFFFFFF, 0.35f));
  txt(String(r.usScore) + "-" + String(r.themScore), 64 - (int)(String(r.usScore) + "-" + String(r.themScore)).length() * 6, 13, 0xFFFFFF);
  txt(b, 64 - (int)b.length() * 6, 23, 0x8090A0);
  gfxEnd();
}

// =============================================================== space station over a day/night map
void drawIssMap(const IssFix &f, time_t now, float homeLat, float homeLon, int frame) {
  double sLat, sLon;
  sunSubpoint(now, sLat, sLon);
  const double R = M_PI / 180;
  double sinS = sin(sLat * R), cosS = cos(sLat * R);
  gfxBegin();
  for (int y = 0; y < 32; y++) {
    double lat = 90 - (y + 0.5) * 180 / 32;
    double sl = sin(lat * R), cl = cos(lat * R);
    uint64_t row = WORLD_LAND[y];
    for (int x = 0; x < 64; x++) {
      double lon = -180 + (x + 0.5) * 360 / 64;
      double cz = sl * sinS + cl * cosS * cos((lon - sLon) * R);  // sine of the sun's altitude
      float day = std::max(0.0, std::min(1.0, (cz + 0.1) / 0.2));  // soft twilight band
      bool land = (row >> (63 - x)) & 1;
      uint32_t c = land ? mix(0x0C2A12, 0x3C9A3C, day) : mix(0x040C24, 0x1350A8, day);
      px(x, y, c);
    }
  }
  if (homeLat || homeLon) {
    int hx = (int)((homeLon + 180) / 360 * 64), hy = (int)((90 - homeLat) / 180 * 32);
    px(hx, hy, 0xFF4040);
  }
  if (f.ok) {
    float mins = (millis() - f.at) / 60000.0f;
    float lon = f.lon + f.dLonPerMin * mins;
    while (lon > 180) lon -= 360;
    while (lon < -180) lon += 360;
    int ix = (int)((lon + 180) / 360 * 64), iy = (int)((90 - f.lat) / 180 * 32);
    uint32_t c = (frame / 8) % 2 ? 0xFFFFFF : 0xFFD040;
    px(ix, iy, c);
    px(ix - 1, iy, scale(c, 0.6f));
    px(ix + 1, iy, scale(c, 0.6f));
    px(ix, iy - 1, scale(c, 0.6f));
    px(ix, iy + 1, scale(c, 0.6f));
  }
  if ((frame / 40) % 4 == 0) {  // now and then, say what it is
    fill(0, 0, 21, 9, 0x000000);
    txt("ISS", 1, 1, 0xFFFFFF);
  }
  gfxEnd();
}

// =============================================================== planets tonight
void drawPlanets(time_t now, float lat, float lon, int page) {
  time_t t = now;
  bool night = sunAltitude(now, lat, lon) < -6;
  if (!night) {  // daytime: show 9 PM tonight instead
    struct tm lt;
    localtime_r(&now, &lt);
    lt.tm_hour = 21;
    lt.tm_min = 0;
    lt.tm_sec = 0;
    t = mktime(&lt);
  }
  std::vector<PlanetPos> ps;
  planetPositions(t, lat, lon, ps);
  std::vector<PlanetPos> up;
  for (auto &p : ps)
    if (p.alt > 3 && !(p.letter == 'M' && sunAltitude(t, lat, lon) > -10)) up.push_back(p);  // Mercury: only in a dark sky
  gfxBegin();
  if (page == 0) {
    for (int y = 0; y < 27; y++) fill(0, y, 64, 1, mix(0x000008, 0x061436, y / 27.0f));
    for (int k = 0; k < 18; k++) px((k * 37 + 11) % 64, (k * 23 + 5) % 24, 0x404050);  // a few faint stars
    fill(0, 27, 64, 5, 0x0C1A0C);
    for (int k = 0; k < 4; k++) {  // compass ticks: N E S W
      int x = (k * 16 + 32) % 64;
      px(x, 27, 0x709070);
    }
    for (auto &p : up) {
      int x = (int)lround(32 + (p.az - 180) / 360 * 64);
      x = (x % 64 + 64) % 64;
      int y = (int)lround(26 - p.alt / 90 * 24);
      fill(x, y, 2, 2, p.color);
      txt(String(p.letter), x + 3 > 58 ? x - 6 : x + 3, std::max(0, y - 3), scale(p.color, 0.9f));
    }
    txt(night ? "NOW" : "9PM", 0, 0, 0x8090B0);
  } else {
    txt(night ? "PLANETS NOW" : "TONIGHT 9PM", 0, 0, 0x8090B0);
    if (up.empty()) txt("none up", 0, 12, 0x707070);
    std::sort(up.begin(), up.end(), [](const PlanetPos &a, const PlanetPos &b) { return a.alt > b.alt; });
    for (size_t i = 0; i < up.size() && i < 3; i++) {
      int y = 9 + (int)i * 8;
      String n = String(up[i].name).substring(0, 4);
      txt(n, 0, y, up[i].color);
      String where = String(compass8(up[i].az)) + " " + String((int)lround(up[i].alt));
      txt(where, 64 - (int)where.length() * 6, y, 0xC0C0C0);
    }
  }
  gfxEnd();
}

// =============================================================== surf report
void drawSurf(const SurfReport &s, const String &name, int frame) {
  gfxBegin();
  txt("SURF", 0, 0, 0xFFFFFF);
  String n = name.substring(0, 6);
  txt(n, 64 - (int)n.length() * 6, 0, 0x40B8FF);
  if (!s.ok) {
    txt("no data", 0, 12, 0x707070);
    gfxEnd();
    return;
  }
  char h[8];
  int lo = (int)floorf(s.waveFt);
  if (s.waveFt < 1) snprintf(h, sizeof h, "<1");
  else snprintf(h, sizeof h, "%d-%d", lo, lo + 1);  // surfers think in ranges: "3-4 ft"
  txt(h, 0, 9, 0x9FE8FF, 2);
  int hw = (int)strlen(h) * 12;
  txt("FT", hw, 16, 0x9FE8FF);
  String per = String((int)lroundf(s.periodS)) + "S";
  txt(per, 64 - (int)per.length() * 6, 9, 0xFFFFFF);
  String dir = compass8(s.dirDeg);
  txt(dir, 64 - (int)dir.length() * 6, 17, 0xC0C0C0);
  if (s.waterF > -100) txt("H2O " + String(s.waterF) + "F", 0, 25, 0x70A0C0);
  for (int x = 44; x < 64; x++) {  // a little rolling wave
    int y = 28 + (int)lroundf(sinf(x * 0.5f - frame * 0.25f) * 1.5f);
    px(x, y, 0x40B8FF);
    px(x, y + 1, 0x1060A0);
  }
  gfxEnd();
}
