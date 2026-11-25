// render5a.cpp - batch 5 clocks: split-flap, analog+dials, terminal, slot machine, life in weeks.
#include <time.h>
#include "gfxutil.h"
#include "render5.h"

using namespace gu;

static const char *DOW3[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char *MON3[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DECADE_COL"};

// 5x7 digit bitmaps (bit 4 = left column), drawn at 2x for the big digits
static const uint8_t DIG[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}};

// pixel of a digit glyph at 2x scale (sx 0..9, sy 0..13); d > 9 = blank
static inline bool big(int d, int sx, int sy) {
  if (d < 0 || d > 9 || sx < 0 || sy < 0 || sx > 9 || sy > 13) return false;
  return (DIG[d][sy >> 1] >> (4 - (sx >> 1))) & 1;
}

static int hour12(int h) {
  if (!CFG.twelveHour) return h;
  h %= 12;
  return h ? h : 12;
}

static String dateStr(const struct tm &t) {
  char b[16];
  snprintf(b, sizeof b, "%s %s %d", DOW3[t.tm_wday], MON3[t.tm_mon], t.tm_mday);
  return b;
}

// ================================================================ 1. split-flap clock
static const int FW = 13, FH = 18, FLIPN = 8;

// one pixel of a flap tile showing digit d at tile coords (tx, ty)
static inline uint32_t flapPix(int d, int tx, int ty, float shade) {
  bool on = big(d, tx - 2, ty - 2);
  return on ? scale(0xFFC830, shade) : scale(0x2C2C2C, 0.6f + 0.4f * shade);
}

static void flapTile(int x0, int y0, int cur, int old, float p) {
  bool flip = p >= 0;
  int nd = cur, od = flip ? old : cur;
  int fh = 0;
  bool topFlap = false;
  if (flip) {
    if (p < 0.5f) fh = (int)lroundf(9 * (1 - 2 * p)), topFlap = true;
    else fh = (int)lroundf(9 * (2 * p - 1));
  }
  for (int ty = 0; ty < FH; ty++)
    for (int tx = 0; tx < FW; tx++) {
      if ((tx == 0 || tx == FW - 1) && (ty == 0 || ty == FH - 1)) continue;
      uint32_t c;
      if (ty < 9) c = flapPix(nd, tx, ty, 1.0f);
      else c = flapPix(od, tx, ty, 1.0f);
      if (flip && fh > 0) {
        if (topFlap && ty >= 9 - fh && ty <= 8) {
          int k = (8 - ty) * 9 / fh;
          c = flapPix(od, tx, 8 - k, 0.55f + 0.45f * fh / 9.0f);
        } else if (!topFlap && ty >= 9 && ty < 9 + fh) {
          int k = (ty - 9) * 9 / fh;
          c = flapPix(nd, tx, 9 + k, 0.55f + 0.45f * fh / 9.0f);
        }
      }
      if (ty == 9) c = 0x000000;  // the split
      px(x0 + tx, y0 + ty, c);
    }
  // hinge pins
  px(x0, y0 + 9, 0x707070);
  px(x0 + FW - 1, y0 + 9, 0x707070);
}

bool drawFlapClock(time_t now, const String &sub, uint32_t subColor, int frame) {
  static int prev[4], oldD[4], startF[4];
  static int lastFrame = -1;
  struct tm t;
  localtime_r(&now, &t);
  int h = hour12(t.tm_hour);
  int d[4] = {h / 10, h % 10, t.tm_min / 10, t.tm_min % 10};
  if (CFG.twelveHour && d[0] == 0) d[0] = 10;  // blank tile
  bool started = false;
  if (frame == 0 || frame < lastFrame || lastFrame < 0) {
    for (int i = 0; i < 4; i++) prev[i] = oldD[i] = d[i], startF[i] = -1000;
  }
  lastFrame = frame;
  for (int i = 0; i < 4; i++)
    if (d[i] != prev[i]) {
      oldD[i] = prev[i];
      prev[i] = d[i];
      startF[i] = frame;
      started = true;
    }
  gfxBegin();
  static const int X[4] = {2, 16, 34, 48};
  for (int i = 0; i < 4; i++) {
    float p = (float)(frame - startF[i]) / FLIPN;
    flapTile(X[i], 1, d[i], oldD[i], (p >= 0 && p < 1) ? p : -1);
  }
  // colon, blinks with the seconds
  if (t.tm_sec % 2 == 0) {
    fill(31, 6, 2, 2, 0xFFC830);
    fill(31, 12, 2, 2, 0xFFC830);
  } else {
    fill(31, 6, 2, 2, 0x504010);
    fill(31, 12, 2, 2, 0x504010);
  }
  // second row
  String s = sub.length() ? sub : dateStr(t);
  uint32_t col = sub.length() ? (subColor ? subColor : 0xFFC830) : 0x60D0FF;
  int w = txtW(s);
  int y = 22;
  if (w > 64) {
    int cyc = w + 24;
    int off = (frame / 2) % cyc;
    txt(s, 64 - off + 0, y, col);
    if (64 - off + cyc < 64) txt(s, 64 - off + cyc, y, col);
  } else {
    txt(s, (64 - w) / 2, y, col);
  }
  gfxEnd();
  return started;
}

// ================================================================ 2. analog + dials
static void hand(int cx, int cy, float deg, float len, uint32_t c, bool thick) {
  float a = deg * (float)M_PI / 180;
  float sx = sinf(a), cyy = -cosf(a);
  int ex = cx + (int)lroundf(len * sx), ey = cy + (int)lroundf(len * cyy);
  line(cx, cy, ex, ey, c);
  if (thick) {
    if (fabsf(cyy) > fabsf(sx)) line(cx + 1, cy, ex + 1, ey, c);
    else line(cx, cy + 1, ex, ey + 1, c);
  }
}

// dotted arc: angles in degrees clockwise from 12 o'clock
static void arcDots(int cx, int cy, float r, float a0, float a1, uint32_t c) {
  for (float a = a0; a <= a1 + 0.01f; a += 4) {
    float t = a * (float)M_PI / 180;
    px((int)floorf(cx + 0.5f + r * sinf(t)), (int)floorf(cy + 0.5f - r * cosf(t)), c);
  }
}

void drawDialsClock(time_t now, const Weather &w, time_t gameStart, const String &gameAbbr, uint32_t gameColor) {
  struct tm t;
  localtime_r(&now, &t);
  gfxBegin();
  // ---- analog face (centre pixel 15,15; radius 15)
  const int C = 15;
  ring(C + 0.5f, C + 0.5f, 15.5f, 0x38384C);
  for (int i = 0; i < 12; i++) {
    float a = i * 30 * (float)M_PI / 180;
    bool q = i % 3 == 0;
    float r0 = q ? 11.5f : 13.5f;
    for (float r = r0; r <= 14.6f; r += 0.5f)
      px((int)floorf(C + 0.5f + r * sinf(a)), (int)floorf(C + 0.5f - r * cosf(a)), q ? 0xFFC830 : 0x9090B0);
  }
  float mm = t.tm_min + t.tm_sec / 60.0f;
  hand(C, C, ((t.tm_hour % 12) + mm / 60.0f) * 30, 7.5f, 0xFFFFFF, true);
  hand(C, C, mm * 6, 11.5f, 0x60D0FF, true);
  float sa = t.tm_sec * 6;
  hand(C, C, sa + 180, 3, 0xFF3030, false);
  hand(C, C, sa, 13, 0xFF3030, false);
  disc(C + 0.5f, C + 0.5f, 1.6f, 0xFF3030);

  // ---- temperature dial (centre 40,8)
  {
    const int cx = 40, cy = 8;
    float lo = (float)w.lo, hi = (float)w.hi;
    if (!w.ok || hi <= lo) lo = (float)w.temp - 10, hi = (float)w.temp + 10;
    float f = w.ok ? std::max(0.0f, std::min(1.0f, (w.temp - lo) / (hi - lo))) : 0;
    float cur = -135 + 270 * f;
    for (float a = -135; a <= 135.01f; a += 4) {
      float k = (a + 135) / 270;
      uint32_t base = hsv(0.62f - 0.62f * k, 1.0f, 1.0f);
      bool lit = w.ok && a <= cur;
      float tt = a * (float)M_PI / 180;
      px((int)floorf(cx + 0.5f + 7 * sinf(tt)), (int)floorf(cy + 0.5f - 7 * cosf(tt)), lit ? base : scale(base, 0.3f));
    }
    if (w.ok) {
      float tt = cur * (float)M_PI / 180;
      for (float r = 4.5f; r <= 7.6f; r += 0.5f)
        px((int)floorf(cx + 0.5f + r * sinf(tt)), (int)floorf(cy + 0.5f - r * cosf(tt)), 0xFFFFFF);
      String s = String(w.temp);
      text3(s, cx - (text3Width(s) - 1) / 2 - 0, cy - 2, 0xFFFFFF);
    } else {
      text3("--", cx - 3, cy - 2, 0x909090);
    }
  }
  // ---- date dial (centre 56,8): ring fills with the month, day number inside
  {
    const int cx = 56, cy = 8;
    static const int DIM[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    float f = (float)t.tm_mday / DIM[t.tm_mon];
    arcDots(cx, cy, 7, 0, 360 - 4, 0x283C60);
    arcDots(cx, cy, 7, 0, 360 * f - 4, 0x40A0FF);
    String s = String(t.tm_mday);
    txt(s, cx - (txtW(s) - 1) / 2 - 0, cy - 3, 0xFFFFFF);
  }
  // ---- next game ring (centre 40,24)
  {
    const int cx = 40, cy = 24;
    arcDots(cx, cy, 7, 0, 360 - 4, 0x303040);
    if (gameStart <= 0) {
      text3("NO", cx - 3, cy - 5, 0x909090);
      text3("GM", cx - 3, cy + 1, 0x909090);
    } else {
      long rem = (long)(gameStart - now);
      float f = rem <= 0 ? 1.0f : 1.0f - (float)rem / (72 * 3600.0f);
      f = std::max(0.0f, std::min(1.0f, f));
      arcDots(cx, cy, 7, 0, 360 * f - 4 + (f >= 1 ? 4 : 0), gameColor);
      arcDots(cx, cy, 6, 0, 360 * f - 4 + (f >= 1 ? 4 : 0), scale(gameColor, 0.45f));
      String s;
      if (rem <= 0) s = "NOW";
      else if (rem >= 86400) s = String(rem / 86400) + "D";
      else if (rem >= 3600) s = String(rem / 3600) + "H";
      else s = String(std::max(1L, rem / 60)) + "M";
      text3(s, cx - (text3Width(s) - 1) / 2, cy - 2, 0xFFFFFF);
    }
  }
  // ---- right column text: weekday + game abbreviation
  text3(DOW3[t.tm_wday], 49, 18, 0xFFC830);
  if (gameStart > 0) {
    String a = gameAbbr;
    if (a.length() > 3) a = a.substring(0, 3);
    text3(a, 49, 25, gameColor);
  }
  gfxEnd();
}

// ================================================================ 3. terminal clock
static const uint32_t TG = 0x33FF66, TG_DIM = 0x22C050, TG_HI = 0xB8FFCC;

static void typed(const String &s, int x, int y, uint32_t c, float n, int *curX = nullptr, int *curY = nullptr) {
  int len = (int)s.length();
  int k = std::max(0, std::min(len, (int)n));
  if (n <= 0) return;
  txt(s.substring(0, k), x, y, c);
  if (k < len && curX) *curX = x + k * 6, *curY = y;
}

void drawTerminalClock(time_t now, int frame) {
  static bool wasFake = false;
  static int fakeF0 = 0, fakeIdx = 0;
  static const char *CMD[5] = {"$ uptime", "$ whoami", "$ pwd", "$ fortune", "$ ping nfl"};
  static const char *OUT[5] = {"up 4d 11h", "pressbox", "/home/clk", "go birds", "64B 12ms"};
  struct tm t;
  localtime_r(&now, &t);
  char b[16];
  if (CFG.twelveHour) snprintf(b, sizeof b, "%d:%02d:%02d%c", hour12(t.tm_hour), t.tm_min, t.tm_sec, t.tm_hour < 12 ? 'A' : 'P');
  else snprintf(b, sizeof b, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
  String tm = b, ds = dateStr(t);
  bool fake = t.tm_sec >= 20 && t.tm_sec < 44;
  if (frame == 0) wasFake = false;
  if (fake && !wasFake) fakeF0 = frame, fakeIdx = (int)((now / 60) % 5);
  wasFake = fake;

  gfxBegin();
  int cx = -1, cy = -1;
  const char spin[4] = {'|', '/', '-', '\\'};
  if (!fake) {
    typed("$ date", 0, 0, TG, frame * 0.5f + 1, &cx, &cy);
    if (frame >= 14) typed(ds, 0, 8, TG_DIM, (frame - 14) * 1.0f + 1, &cx, &cy);
    if (frame >= 26) typed(tm, 0, 16, TG_HI, (frame - 26) * 1.0f + 1, &cx, &cy);
    if (frame >= 38) {
      txt("$", 0, 24, TG);
      cx = 12, cy = 24;
    }
  } else {
    int f = frame - fakeF0;
    txt(tm, 0, 0, TG_HI);
    String cmd = CMD[fakeIdx], out = OUT[fakeIdx];
    int cmdN = (int)cmd.length();
    typed(cmd, 0, 8, TG, f * 0.5f + 1, &cx, &cy);
    int f2 = f - cmdN * 2 - 6;
    if (f2 >= 0) typed(out, 0, 16, TG_DIM, f2 * 1.0f + 1, &cx, &cy);
    int f3 = f2 - (int)out.length() - 4;
    if (f3 >= 0) {
      txt("$", 0, 24, TG);
      cx = 12, cy = 24;
    }
  }
  // spinner top right
  char sp[2] = {spin[t.tm_sec & 3], 0};
  txt(String(sp), 58, 0, TG);
  // cursor
  if (cx >= 0) {
    bool prompt = cy == 24;
    if (!prompt || (frame / 15) % 2 == 0) {
      if (prompt) cx = 8;
      fill(cx, cy, 5, 7, TG);
    }
  }
  gfxEnd();
}

// ================================================================ 4. slot machine
// digit glyph at 2x clipped to window rows [clipY0, clipY1]
static void bigDigit(int d, int x, int y, int clipY0, int clipY1, uint32_t c) {
  for (int sy = 0; sy < 14; sy++) {
    int yy = y + sy;
    if (yy < clipY0 || yy > clipY1) continue;
    for (int sx = 0; sx < 10; sx++)
      if (big(d, sx, sy)) px(x + sx, yy, c);
  }
}

void drawSlotClock(time_t now, int frame) {
  static int f0 = 0;
  static long lastKey = -1;
  static int lastFrame = -1;
  struct tm t;
  localtime_r(&now, &t);
  int h = hour12(t.tm_hour);
  int dg[4] = {h / 10, h % 10, t.tm_min / 10, t.tm_min % 10};
  long key = (long)(now / 60);
  if (frame == 0 || frame < lastFrame || lastKey < 0) f0 = frame, lastKey = key;
  else if (key != lastKey) f0 = frame, lastKey = key;
  lastFrame = frame;
  static const uint32_t COL[4] = {0xFF4040, 0xFFD030, 0x40FF60, 0x40C8FF};
  static const int X[4] = {3, 17, 36, 50};
  const int WY0 = 4, WY1 = 25, RW = 13, RC = 15;  // window rows, width, centre row offset of digit centre
  int el = frame - f0;
  int lastStop = 24 + 14 * 3;
  bool jack = t.tm_min == 0 && el >= lastStop && el < lastStop + 95;
  int jf = el - lastStop;

  gfxBegin();
  // marquee bulbs top and bottom
  for (int i = 0; i < 16; i++) {
    bool on = ((i + frame / 5) % 3) == 0;
    uint32_t c = jack ? hsv((i + frame) / 16.0f, 1, 1) : (on ? 0xFFE060 : 0x504018);
    px(2 + i * 4, 1, c);
    px(2 + i * 4, 30, c);
  }
  uint32_t frameCol = jack && (jf / 3) % 2 ? 0xFFFFFF : 0xFFB000;
  for (int i = 0; i < 4; i++) {
    int x = X[i];
    fill(x, WY0, RW, WY1 - WY0 + 1, 0x14141C);
    // reel
    float r = (float)(24 + 14 * i - el);
    float off = 0;
    if (r > 0) off = r * 0.3f;
    else if (r > -4) off = -0.12f * sinf((float)M_PI * (-r) / 4);
    float pos = dg[i] + off;
    int n0 = (int)floorf(pos);
    uint32_t c = COL[i];
    bool spinning = r > 0;
    if (jack) c = (jf / 3) % 2 ? 0xFFFFFF : c;
    for (int n = n0 - 1; n <= n0 + 2; n++) {
      float yc = (WY0 + WY1) / 2.0f + 0.5f - (n - pos) * 16;
      int d = ((n % 10) + 10) % 10;
      float dist = fabsf(n - pos);
      uint32_t cc = spinning ? scale(c, 0.8f) : c;
      if (dist > 0.5f) cc = scale(cc, dist > 1.0f ? 0.35f : 0.55f);
      bigDigit(d, x + 1 + (RW - 12) / 2, (int)lroundf(yc) - 7, WY0, WY1, cc);
    }
    // glass shading: dark bands top/bottom
    for (int k = 0; k < 3; k++) {
      uint32_t sh = scale(0x14141C, 1.0f);
      (void)sh;
    }
    rect(x - 1, WY0 - 1, RW + 2, WY1 - WY0 + 3, frameCol);
  }
  (void)RC;
  // colon
  fill(31, 11, 2, 2, 0xFFD030);
  fill(31, 18, 2, 2, 0xFFD030);
  if (jack) {
    if ((jf / 4) % 2 == 0) text3("JACKPOT", 18, 27, 0xFFFFFF);
    else text3("JACKPOT", 18, 27, 0xFFB000);
    for (int i = 0; i < 12; i++) {
      uint32_t hh = hash32(i * 7919 + 13);
      int cxp = (int)(hh % 62) + 1;
      int start = (int)((hh >> 8) % 30);
      float y = (jf - start) * (1.0f + ((hh >> 16) % 8) * 0.12f) - 4;
      if (jf < start || y > 33) continue;
      int yy = (int)y;
      fill(cxp, yy, 3, 3, 0xFFC800);
      px(cxp + 1, yy + 1, 0xFFF0A0);
    }
  }
  gfxEnd();
}

// ================================================================ 5. life in weeks
void drawLifeWeeks(time_t now, int birthYear, int birthMonth, int birthDay, int lifeYears, int frame) {
  gfxBegin();
  if (birthYear < 1900 || lifeYears < 10) {
    t3C("SET BIRTHDAY", 13, 0xFFC830);
    gfxEnd();
    return;
  }
  lifeYears = std::min(lifeYears, 100);
  struct tm bt = {};
  bt.tm_year = birthYear - 1900;
  bt.tm_mon = std::max(1, std::min(12, birthMonth)) - 1;
  bt.tm_mday = std::max(1, std::min(31, birthDay));
  bt.tm_hour = 12;
  bt.tm_isdst = -1;
  time_t bd = mktime(&bt);
  long days = (long)((now - bd) / 86400);
  if (days < 0) days = 0;
  long weeks = days / 7;
  long total = (long)lifeYears * 52;
  if (weeks > total) weeks = total;
  int pct = (int)(weeks * 100 / total);
  // grid: one pixel = one month (52/12 weeks), a year is 12 pixels, 5 years per row, a decade = 2 rows
  int decades = (lifeYears + 9) / 10;
  int gap = decades * 3 - 1 <= 26 ? 1 : 0;
  long cells = (long)lifeYears * 12;
  long livedCell = weeks * 12 / 52;
  static const uint32_t DECADE_COL[10] = {0x40C8FF, 0x40E0A0, 0x80E040, 0xE8E040, 0xFFA030,
                                   0xFF6040, 0xE850A0, 0xA060FF, 0x8080FF, 0xC0C0C0};
  int gridBottom = 0;
  bool blinkOn = (frame / 12) % 2 == 0;
  for (long c = 0; c < cells; c++) {
    int yr = (int)(c / 12), mo = (int)(c % 12);
    int dec = yr / 10, yIn = yr % 10;
    int row = dec * (2 + gap) + yIn / 5;
    int x = 2 + (yIn % 5) * 12 + mo, y = row;
    uint32_t col;
    if (c < livedCell) {
      col = DECADE_COL[dec % 10];
      if (yr % 2) col = scale(col, 0.55f);
    } else if (c == livedCell && weeks < total) {
      col = blinkOn ? 0xFFFFFF : DECADE_COL[dec % 10];
    } else {
      col = (yr % 2) ? 0x24242C : 0x30303A;
    }
    px(x, y, col);
    gridBottom = std::max(gridBottom, y);
  }
  // caption
  char cap[24];
  snprintf(cap, sizeof cap, "%ld WEEKS %d%%", weeks, pct);
  int cy = std::min(26, gridBottom + 3);
  t3C(cap, cy, 0xFFFFFF);
  gfxEnd();
}
