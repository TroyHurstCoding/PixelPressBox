// render5b.cpp - toys: robot eyes, fortune ball, party tools (dice / coin / spinner), sand timer, maze.
// All draw functions render one whole frame (gfxBegin ... gfxEnd) and keep animation state in statics.
#include <Arduino.h>
#include "gfxutil.h"
#include "render5.h"

using namespace gu;

// ------------------------------------------------------------------ tiny PRNG (no platform APIs)
static uint32_t g_rs = 0;
static uint32_t rnd() {
  if (!g_rs) g_rs = ((uint32_t)millis() * 2654435761u) | 1u;
  g_rs ^= g_rs << 13;
  g_rs ^= g_rs >> 17;
  g_rs ^= g_rs << 5;
  return g_rs;
}
static int rr(int n) { return n <= 1 ? 0 : (int)(rnd() % (uint32_t)n); }
static void rndMix() {  // fold the clock in so shakes never repeat
  g_rs ^= (uint32_t)millis() * 2246822519u;
  if (!g_rs) g_rs = 1;
}
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

// =================================================================== ROBOT EYES
static float eyePX = 0, eyePY = 0, eyeGX = 0, eyeGY = 0;
static int eyeNextBlink = 60, eyeBlinkT = -1, eyeGlanceT = 0;

static bool eyeShape(float dx, float dy, float hw, float hh, float R) {
  float ax = fabsf(dx), ay = fabsf(dy);
  if (ax > hw || ay > hh) return false;
  float qx = ax - (hw - R), qy = ay - (hh - R);
  if (qx <= 0 || qy <= 0) return true;
  return qx * qx + qy * qy <= R * R;
}

// 9x8 heart for the excited mood
static const uint16_t HEART[8] = {0x063, 0x0F7, 0x1FF, 0x1FF, 0x0FE, 0x07C, 0x038, 0x010};
static bool heartAt(int hx, int hy) {
  if (hx < 0 || hx > 8 || hy < 0 || hy > 7) return false;
  // bit 8 = leftmost column
  return (HEART[hy] >> (8 - hx)) & 1;
}

static void eyeDraw(int cx, int cy, float hw, float hh, float R, float top, float bot, float tilt, int side,
                    float ix, float iy, float ri, float rp, bool heart, int mood, int frame) {
  for (int y = cy - (int)hh - 2; y <= cy + (int)hh + 2; y++)
    for (int x = cx - (int)hw - 2; x <= cx + (int)hw + 2; x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy, fy = y + 0.5f;
      float o = side * dx / hw;  // >0 toward the outer edge of this eye
      float yt = cy - hh + 2 * hh * top + tilt * o;
      float yb = cy + hh - 2 * hh * bot;
      if (yb - yt < 1.0f) yt = cy - 0.6f, yb = cy + 0.6f;
      bool in = eyeShape(dx, dy, hw, hh, R);
      if (!in) {
        if (fy >= yt - 1.2f && fy <= yb + 1.2f && eyeShape(dx, dy, hw + 1.2f, hh + 1.2f, R + 1.2f))
          px(x, y, 0x00506A);  // soft glow around the lid
        continue;
      }
      if (fy < yt || fy > yb) continue;
      bool rim = !eyeShape(dx, dy, hw - 1.0f, hh - 1.0f, R - 1.0f);
      bool lidEdge = (top > 0.04f && fy - yt < 1.0f) || (bot > 0.04f && yb - fy < 1.0f);
      if (rim || lidEdge) {
        px(x, y, 0x00C8E8);
        continue;
      }
      float t = (fy - yt) / std::max(1.0f, yb - yt);
      uint32_t col = mix(0xC8FFFF, 0x40C8E4, t * t * 0.9f + 0.1f * t);
      float ddx = x + 0.5f - (cx + ix), ddy = y + 0.5f - (cy + iy);
      float d = sqrtf(ddx * ddx + ddy * ddy);
      if (heart) {
        int hx = x - (int)floorf(cx + ix + 0.5f) + 4, hy = y - (int)floorf(cy + iy + 0.5f) + 4;
        if (heartAt(hx, hy)) col = (hx + hy < 5 && hy < 3 && hx < 4) ? 0xFF90B8 : 0xFF2060;
      } else if (d <= ri) {
        float k = d / ri;
        col = mix(0x00E8E0, 0x0078E8, k * k);                 // bright teal centre -> deep blue rim
        if (d > ri - 1.1f) col = 0x0058B8;                    // crisp outer ring
        if (d <= rp) col = 0x001428;                          // pupil
        if (ddx > -3.6f && ddx < -1.4f && ddy > -3.8f && ddy < -1.6f) col = 0xFFFFFF;    // highlight
        if (ddx > 2.0f && ddx < 3.2f && ddy > 2.0f && ddy < 3.2f && d > rp + 0.5f) col = 0xB8FFFF;  // glint
      }
      px(x, y, col);
    }
}

static void plus3(int x, int y, uint32_t c) {
  px(x, y, c); px(x - 1, y, c); px(x + 1, y, c); px(x, y - 1, c); px(x, y + 1, c);
}

void drawEyes(int frame, float gx, float gy, int mood) {
  if (frame == 0) {
    eyePX = eyePY = eyeGX = eyeGY = 0;
    eyeBlinkT = -1;
    eyeGlanceT = 0;
    eyeNextBlink = 45 + rr(90);
    rndMix();
  }
  // ---- blink schedule: every 3-7 s, sometimes a quick double
  int blinkLen = mood == 2 ? 14 : 4;
  if (eyeBlinkT < 0 && frame >= eyeNextBlink) eyeBlinkT = 0;
  float b = 0;
  if (eyeBlinkT >= 0) {
    if (mood == 2) {
      int t = eyeBlinkT;
      b = t < 5 ? t / 5.0f : (t < 9 ? 1.0f : 1.0f - (t - 9) / 5.0f);
    } else {
      static const float prof[4] = {0.55f, 1.0f, 1.0f, 0.5f};
      b = prof[std::min(eyeBlinkT, 3)];
    }
    if (++eyeBlinkT >= blinkLen) {
      eyeBlinkT = -1;
      eyeNextBlink = frame + (rr(100) < 12 ? 7 : 90 + rr(120));
    }
  }
  // ---- where the pupils look
  const float maxX = 5.0f, maxY = 4.0f;
  float tx = clampf(gx * 1.5f, -1, 1) * maxX, ty = clampf(gy * 1.5f, -1, 1) * maxY;
  bool still = fabsf(gx) < 0.14f && fabsf(gy) < 0.14f;
  if (still) {
    if (eyeGlanceT > 0) {
      eyeGlanceT--;
      tx = eyeGX; ty = eyeGY;
    } else if (rr(100) < 2) {
      eyeGlanceT = 18 + rr(28);
      eyeGX = (rr(3) - 1) * maxX * 0.9f;
      eyeGY = (rr(3) - 1) * maxY * 0.8f;
      if (eyeGX == 0 && eyeGY == 0) eyeGX = maxX * 0.9f * (rr(2) ? 1 : -1);
    }
  } else {
    eyeGlanceT = 0;
  }
  if (mood == 2) ty += 2.0f;
  if (mood == 1) ty += sinf(frame * 0.45f) * 0.8f;
  float sp = mood == 2 ? 0.12f : 0.35f;
  eyePX += (tx - eyePX) * sp;
  eyePY += (ty - eyePY) * sp;

  gfxBegin();
  float hw = 14, hh = mood == 1 ? 14.0f : 13.0f, R = 9;
  float top = 0, bot = 0, tilt = 0;
  float ri = 8.5f, rp = 4.0f;
  if (mood == 1) { ri = 9.5f; rp = 5.0f; }
  if (mood == 2) { top = 0.42f; ri = 7.8f; rp = 3.6f; tilt = 2.5f; bot = 0.04f; }
  // blink closes both lids toward the middle
  float tp = top + (1 - top) * b * 0.62f, bt = bot + (1 - bot) * b * 0.45f;
  bool hearts = mood == 1 && ((frame / 45) % 3 == 2);
  eyeDraw(15, 16, hw, hh, R, tp, bt, tilt, -1, eyePX, eyePY, ri, rp, hearts, mood, frame);
  eyeDraw(49, 16, hw, hh, R, tp, bt, tilt, +1, eyePX, eyePY, ri, rp, hearts, mood, frame);

  if (mood == 1) {  // twinkling sparkles in the free corners
    for (int i = 0; i < 4; i++) {
      int ph = (frame + i * 9) % 36;
      if (ph < 14) {
        float k = ph < 7 ? ph / 7.0f : (14 - ph) / 7.0f;
        int sx = (i & 1) ? 61 : 2, sy = (i & 2) ? 29 : 2;
        uint32_t c = mix(0x004860, 0xFFFFC0, k);
        if (k > 0.7f) plus3(sx, sy, c); else px(sx, sy, c);
      }
    }
    // little stars in the gap between the eyes
    int ph = (frame * 2) % 40;
    if (ph < 20) px(31 + (ph & 1), 16 - ph / 3 + 3, mix(0x103040, 0xFFFFA0, 1.0f - ph / 20.0f));
  }
  if (mood == 2) {  // drifting z's
    for (int i = 0; i < 2; i++) {
      int ph = (frame + i * 35) % 70;
      float k = ph / 70.0f;
      int zx = 55 + (i ? 3 : 0) + (int)(sinf(ph * 0.15f) * 1.5f), zy = 14 - (int)(k * 13);
      uint32_t c = mix(0xA0D8FF, 0x203860, k);
      text3("Z", zx, zy, c);
    }
  }
  gfxEnd();
}

// =================================================================== FORTUNE BALL
// original answer list: two short lines each (<= 10 / 9 characters)
static const char *const EB_ANS[20][2] = {
    {"SURE", "THING"},     {"ALL SIGNS", "SAY YES"}, {"GO FOR", "IT"},      {"COUNT", "ON IT"},
    {"LOOKS", "GOOD"},     {"ABSO-", "LUTELY"},      {"PROBABLY", "YES"},   {"ASK", "LATER"},
    {"HARD TO", "SAY NOW"}, {"TRY", "AGAIN"},        {"CLOUDY", "OUTLOOK"}, {"FLIP A", "COIN"},
    {"STAY", "TUNED"},     {"DON'T", "BET ON IT"},   {"NO", "WAY"},         {"HIGHLY", "UNLIKELY"},
    {"NOT IN THE", "CARDS"}, {"BETTER", "NOT"},      {"MY GUESS", "IS NO"}, {"NOT", "LIKELY"}};
static int ebAns = -1, ebT0 = -1000;
static bool ebHas = false;

static void eightIdle(int frame) {
  const float cx = 45.5f, cy = 16.0f, R = 15.2f;
  for (int y = 0; y < 32; y++)
    for (int x = 28; x < 64; x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = sqrtf(dx * dx + dy * dy);
      if (d > R) continue;
      float nx = dx / R, ny = dy / R, nz = sqrtf(std::max(0.0f, 1 - nx * nx - ny * ny));
      float l = clampf((-nx * 0.5f - ny * 0.6f + nz * 0.62f), 0, 1);
      uint32_t c = scale(0x9AA8D0, 0.07f + 0.30f * l * l);
      if (d > R - 1.0f) c = 0x4A5478;  // rim light
      px(x, y, c);
    }
  // specular spot
  disc(38.5f, 8.5f, 2.2f, 0x5C6A98);
  disc(38.0f, 8.0f, 1.2f, 0xC8D4FF);
  // "8" badge
  disc(cx, 16.2f, 7.0f, 0xF4F4FF);
  txt("8", 43, 12, 0x000010);
  txt("8", 44, 12, 0x000010);
  // hint
  uint32_t hc = (frame / 15) % 2 ? 0x30D8FF : 0x1890C0;
  txt("SHAKE", 1, 6, hc);
  txt("ME", 1, 16, 0x6090FF);
  text3("TO ASK", 1, 27, 0x4068C0);
  // sparkle on the ball
  int ph = (frame / 2) % 40;
  if (ph < 8) { int k = ph < 4 ? ph : 8 - ph; if (k > 1) plus3(35, 22, 0xFFFFFF); else px(35, 22, 0xA0B0FF); }
}

static void eightAnswer(int t, int frame) {
  // dark liquid window (rounded rect with a bluish frame)
  const float hw = 31.5f, hh = 15.5f, R = 7;
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 64; x++) {
      float dx = x + 0.5f - 32, dy = y + 0.5f - 16;
      if (!eyeShape(dx, dy, hw, hh, R)) continue;
      bool edge = !eyeShape(dx, dy, hw - 1, hh - 1, R - 1);
      float g = (y / 31.0f);
      uint32_t c = edge ? 0x2A48B8 : mix(0x00103C, 0x000824, g);
      px(x, y, c);
    }
  // drifting bubbles / swirl while the answer rises
  for (int i = 0; i < 10; i++) {
    uint32_t h = hash32(i * 977 + 13);
    float sp = 0.35f + (h & 7) * 0.06f;
    int bx = 4 + (int)(h >> 8) % 56;
    int by = 30 - (int)fmodf(frame * sp + (h >> 16) % 40, 30);
    px(bx + (int)(sinf(frame * 0.1f + i) * 1.5f), by, 0x1E3C8C);
  }
  // the triangle: pointing down, rising from below the window
  float u = clampf((t - 4) / 28.0f, 0, 1);
  u = 1 - (1 - u) * (1 - u) * (1 - u);
  int off = (int)((1 - u) * 34);
  const int ty0 = 3, th = 24;
  for (int d = 0; d < th; d++) {
    int y = ty0 + d + off;
    if (y < 1 || y > 30) continue;
    float hwid = 29.5f - 17.0f * d / (th - 1);
    for (int x = (int)floorf(32 - hwid); x < (int)ceilf(32 + hwid); x++) {
      float dx = fabsf(x + 0.5f - 32);
      if (dx > hwid) continue;
      float edgeD = std::min(hwid - dx, (float)d);
      if (!eyeShape(x + 0.5f - 32, y + 0.5f - 16, hw - 1, hh - 1, R - 1)) continue;
      uint32_t c = edgeD < 1.3f ? 0x8CB4FF : mix(0x2048E8, 0x0C28A8, d / (float)th);
      px(x, y, c);
    }
  }
  if (u > 0.55f) {
    float k = clampf((u - 0.55f) / 0.35f, 0, 1);
    uint32_t tc = mix(0x2048E8, 0xFFFFFF, k);
    const char *l1 = EB_ANS[ebAns][0], *l2 = EB_ANS[ebAns][1];
    String s1 = l1, s2 = l2;
    int w1 = text3Width(s1), w2 = text3Width(s2);
    int y1 = ty0 + 5 + off, y2 = ty0 + 13 + off;
    text3(s1, 32 - w1 / 2, y1, tc);
    text3(s2, 32 - w2 / 2, y2, tc);
  }
}

void drawEightBall(int frame, bool shaken) {
  if (frame == 0) {
    ebHas = false;
    ebT0 = -1000;
  }
  if (shaken) {
    rndMix();
    int n;
    do { n = rr(20); } while (n == ebAns);
    ebAns = n;
    ebHas = true;
    ebT0 = frame;
  }
  gfxBegin();
  if (!ebHas) eightIdle(frame);
  else eightAnswer(frame - ebT0, frame);
  gfxEnd();
}

// =================================================================== PARTY TOOLS
static int pDieA = 5, pDieB = 2, pCoin = 0, pWinner = 0;  // coin: 0 heads, 1 tails
static float pRot = 0, pRotStart = 0, pRotTarget = 0;
static int pT0[3] = {-1000, -1000, -1000};
static const int PDICE_LEN = 32, PCOIN_LEN = 38, PSPIN_LEN = 80;

static const uint32_t WHEEL_COL[8] = {0xFF2828, 0xFF8800, 0xFFE000, 0x20E048, 0x00D8E8, 0x3868FF, 0xB048FF, 0xFF58B8};
static const uint32_t WHEEL_TXT[8] = {0xFFFFFF, 0x000000, 0x000000, 0x000000, 0x000000, 0xFFFFFF, 0xFFFFFF, 0x000000};

static void pips(int face, int x, int y, uint32_t c) {
  // 3x3 grid, 2x2 pips at offsets 3, 8, 13 inside an 18x18 die
  static const uint16_t P[7] = {0, 0x010, 0x101, 0x111, 0x145, 0x155, 0x1C7};
  // bit index = row*3+col, row-major from top-left: 0 TL,1 T,2 TR,3 ML,4 C,5 MR,6 BL,7 B,8 BR
  uint16_t m = 0;
  switch (face) {
    case 1: m = 1 << 4; break;
    case 2: m = (1 << 0) | (1 << 8); break;
    case 3: m = (1 << 0) | (1 << 4) | (1 << 8); break;
    case 4: m = (1 << 0) | (1 << 2) | (1 << 6) | (1 << 8); break;
    case 5: m = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 8); break;
    default: m = (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8); break;
  }
  (void)P;
  static const int O[3] = {2, 7, 13};
  for (int i = 0; i < 9; i++)
    if (m & (1 << i)) fill(x + O[i % 3], y + O[i / 3], 3, 3, c);
}

static void drawDie(int x, int y, int face, uint32_t body, uint32_t shade, uint32_t pip) {
  for (int j = 0; j < 18; j++)
    for (int i = 0; i < 18; i++) {
      bool corner = (i < 2 && j < 2 && (i + j) < 2) || (i > 15 && j < 2 && (16 - i + j) < 1 + 0 && false);
      (void)corner;
      int ci = std::min(i, 17 - i), cj = std::min(j, 17 - j);
      if (ci + cj < 2) continue;  // rounded corners
      uint32_t c = body;
      if (i >= 16 || j >= 16) c = shade;
      px(x + i, y + j, c);
    }
  pips(face, x, y, face == 1 ? 0xE01818 : pip);
}

static void partyPanelLabel(const char *lab, int x0, int x1) {
  String s = lab;
  text3(s, (x0 + x1 - text3Width(s)) / 2, 1, 0x40B0FF);
}
static void partyHint(int frame, int x0, int x1) {
  String s = "SHAKE";
  uint32_t c = (frame / 14) % 2 ? 0x6C8CC8 : 0x3C5088;
  text3(s, (x0 + x1 - text3Width(s)) / 2, 26, c);
}

static void drawDice(int frame) {
  int t = frame - pT0[0];
  bool rolling = t >= 0 && t < PDICE_LEN;
  int a = pDieA, b = pDieB, ax = 1, bx = 21, ay = 8, by = 8;
  if (rolling) {
    float k = 1.0f - t / (float)PDICE_LEN;
    int step = t / 3;
    a = 1 + (int)(hash32(step * 7 + 1) % 6);
    b = 1 + (int)(hash32(step * 13 + 5) % 6);
    ax += (int)(((int)(hash32(t * 3 + 1) % 7) - 3) * k);
    bx += (int)(((int)(hash32(t * 5 + 2) % 7) - 3) * k);
    float hop = fabsf(sinf(t * 0.42f)) * 9.0f * k;
    ay = 8 - (int)hop;
    by = 8 - (int)(fabsf(sinf(t * 0.42f + 0.9f)) * 9.0f * k);
  }
  drawDie(ax, ay, a, 0xF2F2FA, 0xA8A8C0, 0x101028);
  drawDie(bx, by, b, 0xFF4848, 0xA81818, 0xFFFFFF);
  partyPanelLabel("DICE", 41, 63);
  if (!rolling) {
    String s = String(pDieA + pDieB);
    int w = (int)s.length() * 12 - 2;
    text3("SUM", 41 + (23 - 11) / 2, 8, 0x7088C0);
    gfxText(s, 41 + (23 - w) / 2, 14, 0xFFD040, 2);
    partyHint(frame, 41, 63);
  } else {
    text3("...", 46, 14, 0x7088C0);
  }
}

static void drawCoin(int frame) {
  int t = frame - pT0[1];
  bool flipping = t >= 0 && t < PCOIN_LEN;
  float u = flipping ? t / (float)PCOIN_LEN : 1.0f;
  float e = 1 - (1 - u) * (1 - u);
  float turns = 4.0f + (pCoin ? 0.5f : 0.0f);
  float ang = flipping ? e * turns * 2 * (float)M_PI : (pCoin ? (float)M_PI : 0.0f);
  float cs = cosf(ang);
  float hop = flipping ? sinf(u * (float)M_PI) * 7.0f : 0.0f;
  const float R = 12.0f, cx = 17.5f, cy = 18.0f - hop;
  float rx = std::max(1.0f, R * fabsf(cs));
  bool heads = cs >= 0;
  float lum = 0.55f + 0.45f * fabsf(cs);
  for (int y = (int)(cy - R) - 1; y <= (int)(cy + R) + 1; y++)
    for (int x = (int)(cx - rx) - 1; x <= (int)(cx + rx) + 1; x++) {
      float dx = (x + 0.5f - cx) / rx * R, dy = y + 0.5f - cy;
      float d = sqrtf(dx * dx + dy * dy);
      if (d > R) continue;
      uint32_t c = 0xFFC820;
      if (d > R - 1.6f) c = 0xFFF090;        // bright rim
      else if (d > R - 3.2f) c = 0xD89400;   // engraved ring
      if (rx < 3) c = 0xFFE060;               // edge-on: bright sliver
      px(x, y, scale(c, lum));
    }
  if (rx > 7.5f) {
    const char *ch = flipping ? (heads ? "H" : "T") : (pCoin ? "T" : "H");
    uint32_t lc = scale(0x7A4800, lum);
    gfxText(ch, (int)(cx - 4.5f), (int)(cy - 6), lc, 2);
  }
  partyPanelLabel("COIN", 34, 63);
  if (!flipping) {
    const char *r = pCoin ? "TAILS" : "HEADS";
    txt(r, 34 + (30 - 29) / 2, 12, pCoin ? 0x50E8FF : 0xFFD040);
    partyHint(frame, 34, 63);
  } else {
    text3("...", 44, 13, 0x7088C0);
  }
}

static int wheelSlice(float rot) {
  float a = fmodf(-rot, 360.0f);
  if (a < 0) a += 360;
  int i = (int)(a / 45.0f);
  return i > 7 ? 7 : i;
}

static void drawWheel(int frame) {
  int t = frame - pT0[2];
  bool spinning = t >= 0 && t < PSPIN_LEN;
  float rot = pRot;
  if (spinning) {
    float u = t / (float)PSPIN_LEN, e = 1 - (1 - u) * (1 - u) * (1 - u);
    rot = pRotStart + (pRotTarget - pRotStart) * e;
  } else if (t >= PSPIN_LEN) {
    rot = pRotTarget;
  }
  pRot = rot;
  const float cx = 17.0f, cy = 16.0f, R = 15.5f;
  for (int y = 0; y < 32; y++)
    for (int x = 1; x < 34; x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = sqrtf(dx * dx + dy * dy);
      if (d > R) continue;
      if (d > R - 1.5f) { px(x, y, 0xD0D8E8); continue; }
      float ang = atan2f(dy, dx) * 180.0f / (float)M_PI - rot;
      ang = fmodf(ang, 360.0f);
      if (ang < 0) ang += 360;
      int idx = (int)(ang / 45.0f);
      if (idx > 7) idx = 7;
      float fr = ang - idx * 45.0f;
      float bd = std::min(fr, 45.0f - fr) * (float)M_PI / 180.0f * d;
      uint32_t c = WHEEL_COL[idx];
      if (bd < 0.55f) c = 0x101018;
      if (d < 3.2f) c = 0xE8E8F0;
      px(x, y, c);
    }
  for (int i = 0; i < 8; i++) {
    float a = (rot + i * 45.0f + 22.5f) * (float)M_PI / 180.0f;
    int tx = (int)floorf(cx + cosf(a) * 9.6f - 1.0f + 0.5f), ty = (int)floorf(cy + sinf(a) * 9.6f - 2.0f + 0.5f);
    text3(String(i + 1), tx, ty, WHEEL_TXT[i]);
  }
  // pointer on the right edge pointing at the wheel
  for (int x = 28; x <= 36; x++) {
    int h = (x - 28) / 2;
    for (int y = 16 - h; y <= 15 + h + 0; y++) px(x, y, 0xFFFFFF);
  }
  px(28, 15, 0xFF2020); px(28, 16, 0xFF2020); px(29, 15, 0xFF2020); px(29, 16, 0xFF2020);
  partyPanelLabel("SPIN", 38, 63);
  int idx = wheelSlice(rot);
  int boxX = 44, boxW = 14;
  fill(boxX, 8, boxW, 17, WHEEL_COL[idx]);
  gfxText(String(idx + 1), boxX + 2, 10, WHEEL_TXT[idx] ? WHEEL_TXT[idx] : 0x000000, 2);
  if (!spinning) partyHint(frame, 38, 63);
}

void drawParty(int tool, int frame, bool shaken) {
  if (tool < 0 || tool > 2) tool = 0;
  if (frame == 0) pT0[0] = pT0[1] = pT0[2] = -1000;
  if (shaken) {
    rndMix();
    pT0[tool] = frame;
    if (tool == 0) {
      pDieA = 1 + rr(6);
      pDieB = 1 + rr(6);
    } else if (tool == 1) {
      pCoin = rr(2);
    } else {
      pWinner = rr(8);
      float off = 8.0f + rr(30);
      float fin = fmodf(-(pWinner * 45.0f + off), 360.0f);
      if (fin < 0) fin += 360;
      float curm = fmodf(pRot, 360.0f);
      if (curm < 0) curm += 360;
      float dm = fin - curm;
      if (dm < 0) dm += 360;
      pRotStart = pRot;
      pRotTarget = pRot + 360.0f * (3 + rr(2)) + dm;
    }
  }
  gfxBegin();
  if (tool == 0) drawDice(frame);
  else if (tool == 1) drawCoin(frame);
  else drawWheel(frame);
  gfxEnd();
}

// =================================================================== SAND TIMER
static const uint8_t DIG3[10][5] = {{7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
                                    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};
static void bigDigit(int d, int x, int y, int s, uint32_t c) {
  if (d < 0 || d > 9) return;
  for (int r = 0; r < 5; r++)
    for (int k = 0; k < 3; k++)
      if (DIG3[d][r] & (1 << (2 - k))) fill(x + k * s, y + r * s, s, s, c);
}
// M:SS in chunky digits, left edge x0; returns nothing. mins < 10 -> scale 3, else scale 2.
static void bigTime(int sec, int x0, int y, uint32_t c, bool colonOn) {
  if (sec < 0) sec = 0;
  int m = std::min(sec / 60, 99), s = sec % 60;
  if (m < 10) {
    bigDigit(m, x0, y, 3, c);
    if (colonOn) { fill(x0 + 12, y + 3, 3, 3, c); fill(x0 + 12, y + 9, 3, 3, c); }
    bigDigit(s / 10, x0 + 17, y, 3, c);
    bigDigit(s % 10, x0 + 28, y, 3, c);
  } else {
    bigDigit(m / 10, x0, y + 2, 2, c);
    bigDigit(m % 10, x0 + 8, y + 2, 2, c);
    if (colonOn) { fill(x0 + 16, y + 4, 2, 2, c); fill(x0 + 16, y + 8, 2, 2, c); }
    bigDigit(s / 10, x0 + 20, y + 2, 2, c);
    bigDigit(s % 10, x0 + 28, y + 2, 2, c);
  }
}

static int glassK(int y) {  // half width of the inside of the top bulb at row y (2..15); mirrored for the bottom
  float hw = y <= 4 ? 8.0f : 8.0f - (y - 4) * 0.68f;
  return hw < 0 ? 0 : (int)hw;
}
static int glassRowK(int y) { return y <= 15 ? glassK(y) : glassK(31 - y); }

static void text3eq(int x, int y, uint32_t c) {  // "WAIT=GO"
  int w = text3Width("WAIT");
  text3("WAIT", x, y, c);
  fill(x + w + 1, y + 1, 3, 1, c);
  fill(x + w + 1, y + 3, 3, 1, c);
  text3("GO", x + w + 6, y, c);
}

static void hourglass(float f, bool running, int frame, uint32_t glassCol) {
  const int cx = 10;
  // frame: caps and posts
  for (int x = 0; x <= 20; x++) {
    px(x, 0, 0xC89040); px(x, 1, 0x9A6A28);
    px(x, 30, 0x9A6A28); px(x, 31, 0xC89040);
  }
  // glass outline
  for (int y = 2; y <= 29; y++) {
    int k = glassRowK(y);
    px(cx - k - 1, y, glassCol);
    px(cx + k + 1, y, glassCol);
  }
  // top sand
  int topArea = 0;
  for (int y = 2; y <= 15; y++) topArea += 2 * glassK(y) + 1;
  float need = f * topArea;
  float acc = 0;
  for (int y = 15; y >= 2 && need - acc > 0.001f; y--) {
    int w = 2 * glassK(y) + 1;
    float take = std::min((float)w, need - acc);
    int n = (int)floorf(take + 0.5f);
    if (n < 1 && take > 0.001f) n = 1;
    for (int i = 0; i < n; i++) px(cx - n / 2 + i + (n % 2 == 0 ? 0 : 0), y, y == 15 ? 0xFFD060 : 0xFFB420);
    acc += take;
  }
  // bottom sand: fills from the floor up
  need = (1 - f) * topArea;
  acc = 0;
  int surf = 30;
  for (int y = 29; y >= 16 && need - acc > 0.001f; y--) {
    int w = 2 * glassRowK(y) + 1;
    float take = std::min((float)w, need - acc);
    int n = (int)floorf(take + 0.5f);
    if (n < 1) n = 1;
    for (int i = 0; i < n; i++) px(cx - n / 2 + i, y, (acc + take >= need - 0.001f) ? 0xFFD060 : 0xFFB420);
    acc += take;
    surf = y;
  }
  // falling stream
  if (running && f > 0.004f) {
    for (int y = 16; y < surf; y++)
      if (((y + frame) % 3) == 0) px(cx, y, 0xFFE890);
    px(cx, 15, 0xFFE890);
  }
}

void drawSandTimer(int totalSec, int remainSec, int setSec, int frame) {
  gfxBegin();
  bool setting = totalSec <= 0;
  bool done = !setting && remainSec <= 0;
  if (setting) {
    hourglass(1.0f, false, frame, 0x70C0F0);
    bigTime(setSec, 24, 1, 0x60F0FF, true);
    text3("+15S TAP", 24, 19, 0x40C8FF);
    text3eq(24, 26, 0x90FF70);
  } else if (!done) {
    float f = clampf(remainSec / (float)totalSec, 0, 1);
    hourglass(f, true, frame, 0x70C0F0);
    uint32_t tc = 0xFFD070;
    if (remainSec <= 10) tc = (frame / 8) % 2 ? 0xFF4030 : 0xFFA040;
    bool colon = true;
    bigTime(remainSec, 24, 2, tc, colon);
    // progress bar (remaining)
    fill(24, 21, 39, 4, 0x1C2434);
    int w = (int)floorf(f * 39 + 0.5f);
    uint32_t bc = f > 0.5f ? 0x20E060 : (f > 0.2f ? 0xFFB000 : 0xFF3020);
    fill(24, 21, w, 4, bc);
    if (w > 1) fill(24, 21, w, 1, mix(bc, 0xFFFFFF, 0.35f));
    String tot = String(totalSec / 60) + ":" + (totalSec % 60 < 10 ? "0" : "") + String(totalSec % 60);
    text3("OF " + tot, 24, 27, 0x6C80A8);
  } else {
    hourglass(0.0f, false, frame, 0x70C0F0);
    bool on = (frame / 8) % 2 == 0;
    uint32_t c = on ? 0xFF3020 : 0xFFD040;
    String s = "TIME!";
    int x = 24 + (40 - ((int)s.length() * 6 - 1)) / 2;
    if (on || true) {
      txt(s, x, 7, c);
      txt(s, x + 1, 7, c);
    }
    fill(25, 18, 38, 2, on ? 0xFF3020 : 0x502010);
    text3("0:00", 24 + (40 - 15) / 2, 24, on ? 0xFFFFFF : 0x7080A0);
  }
  gfxEnd();
}

// =================================================================== MAZE
#define MW 31
#define MH 15
enum { MZ_R = 1, MZ_D = 2, MZ_GEN = 4, MZ_SEEN = 8, MZ_PATH = 16 };
static uint8_t mz[MH][MW];       // bits above; bits 5-6 = direction to the BFS parent
static uint16_t mq[MW * MH];     // DFS stack while carving, BFS queue while solving
static int mqh = 0, mqt = 0;     // queue head/tail (mqt is the stack pointer while carving)
static int mPhase = 0, mFrame = 0, mHold = 0, mTrace = 0;
static const int MDX[4] = {1, 0, -1, 0}, MDY[4] = {0, 1, 0, -1};  // 0 R, 1 D, 2 L, 3 U

static bool mOpen(int x, int y, int d) {
  switch (d) {
    case 0: return x < MW - 1 && (mz[y][x] & MZ_R);
    case 1: return y < MH - 1 && (mz[y][x] & MZ_D);
    case 2: return x > 0 && (mz[y][x - 1] & MZ_R);
    default: return y > 0 && (mz[y - 1][x] & MZ_D);
  }
}
static void mazeReset() {
  memset(mz, 0, sizeof(mz));
  rndMix();
  int sx = rr(MW), sy = rr(MH);
  mz[sy][sx] |= MZ_GEN;
  mq[0] = (uint16_t)(sy * MW + sx);
  mqt = 1;
  mqh = 0;
  mPhase = 0;
  mHold = 0;
  mTrace = MW * MH - 1;
}
static bool mazeCarve() {  // carve one more cell (backtracking is free); false once the maze is complete
  while (mqt > 0) {
    int c = mq[mqt - 1], x = c % MW, y = c / MW, nb[4], n = 0;
    for (int d = 0; d < 4; d++) {
      int nx = x + MDX[d], ny = y + MDY[d];
      if (nx >= 0 && ny >= 0 && nx < MW && ny < MH && !(mz[ny][nx] & MZ_GEN)) nb[n++] = d;
    }
    if (!n) { mqt--; continue; }
    int d = nb[rr(n)], nx = x + MDX[d], ny = y + MDY[d];
    if (d == 0) mz[y][x] |= MZ_R;
    else if (d == 1) mz[y][x] |= MZ_D;
    else if (d == 2) mz[ny][nx] |= MZ_R;
    else mz[ny][nx] |= MZ_D;
    mz[ny][nx] |= MZ_GEN;
    mq[mqt++] = (uint16_t)(ny * MW + nx);
    return true;
  }
  return false;
}
static void mazeSolveStart() {
  mqh = 0; mqt = 0;
  mq[mqt++] = 0;
  mz[0][0] |= MZ_SEEN;
  mPhase = 1;
}
static bool mazeFlood() {  // one BFS expansion; true when the exit has been reached
  if (mqh >= mqt) return true;
  int c = mq[mqh++], x = c % MW, y = c / MW;
  for (int d = 0; d < 4; d++) {
    if (!mOpen(x, y, d)) continue;
    int nx = x + MDX[d], ny = y + MDY[d];
    if (mz[ny][nx] & MZ_SEEN) continue;
    mz[ny][nx] |= MZ_SEEN;
    mz[ny][nx] = (uint8_t)((mz[ny][nx] & 0x9F) | (((d + 2) & 3) << 5));
    mq[mqt++] = (uint16_t)(ny * MW + nx);
    if (nx == MW - 1 && ny == MH - 1) return true;
  }
  return false;
}

static uint32_t mazePathCol(int x, int y, int frame) {
  // golden path with bright pulses running along it
  float w = 0.5f + 0.5f * sinf((x + y) * 0.5f - frame * 0.35f);
  return mix(0xFF7800, 0xFFFFB0, w * w);
}

void drawMaze(int frame) {
  if (frame == 0) {
    mazeReset();
    mFrame = 0;
  }
  mFrame++;
  // ---- advance the animation
  if (mPhase == 0) {
    for (int i = 0; i < 4; i++)
      if (!mazeCarve()) { mazeSolveStart(); break; }
  } else if (mPhase == 1) {
    bool found = false;
    for (int i = 0; i < 7 && !found; i++) found = mazeFlood();
    if (found) mPhase = 2;
  } else if (mPhase == 2) {
    for (int i = 0; i < 4; i++) {
      int x = mTrace % MW, y = mTrace / MW;
      mz[y][x] |= MZ_PATH;
      if (mTrace == 0) { mPhase = 3; break; }
      int d = (mz[y][x] >> 5) & 3;
      mTrace = (y + MDY[d]) * MW + (x + MDX[d]);
    }
  } else {
    if (++mHold > 100) { mazeReset(); }
  }

  gfxBegin();
  const uint32_t WALL = 0x2C44D8, FLOOD = 0x0C4C5E;
  fill(0, 0, 63, 31, WALL);
  for (int y = 0; y < MH; y++)
    for (int x = 0; x < MW; x++) {
      uint8_t v = mz[y][x];
      if (!(v & MZ_GEN)) continue;
      int sx = 2 * x + 1, sy = 2 * y + 1;
      uint32_t cc = 0x000000;
      if (v & MZ_PATH) cc = mazePathCol(sx, sy, mFrame);
      else if ((v & MZ_SEEN) && mPhase < 3) cc = FLOOD;
      px(sx, sy, cc);
      if (v & MZ_R) {
        uint8_t w = mz[y][x + 1];
        uint32_t ec = 0;
        if ((v & MZ_PATH) && (w & MZ_PATH)) ec = mazePathCol(sx + 1, sy, mFrame);
        else if ((v & MZ_SEEN) && (w & MZ_SEEN) && mPhase < 3) ec = FLOOD;
        px(sx + 1, sy, ec);
      }
      if (v & MZ_D) {
        uint8_t w = mz[y + 1][x];
        uint32_t ec = 0;
        if ((v & MZ_PATH) && (w & MZ_PATH)) ec = mazePathCol(sx, sy + 1, mFrame);
        else if ((v & MZ_SEEN) && (w & MZ_SEEN) && mPhase < 3) ec = FLOOD;
        px(sx, sy + 1, ec);
      }
    }
  if (mPhase == 1) {  // bright flood front
    for (int i = std::max(0, mqh - 7); i < mqh; i++) {
      int c = mq[i], x = c % MW, y = c / MW;
      px(2 * x + 1, 2 * y + 1, 0x70FFFF);
    }
  }
  if (mPhase == 0 && mqt > 0) {  // carving head
    int c = mq[mqt - 1], x = c % MW, y = c / MW;
    px(2 * x + 1, 2 * y + 1, (mFrame / 2) % 2 ? 0xFFFFFF : 0xFFE060);
  }
  // entrance and exit gaps
  bool blink = (mFrame / 8) % 2 == 0;
  px(0, 1, 0x30FF50);
  px(62, 29, blink ? 0xFF4040 : 0xFF9090);
  if (mPhase >= 2) {  // after the solve: start/end markers on the path
    px(1, 1, 0xFFFFFF);
  }
  gfxEnd();
}
