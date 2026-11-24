#include "render3.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "astro.h"
#include "desk.h"
#include "render.h"

// helpers exported by render.cpp
MatrixPanel_I2S_DMA *gfxPanel();
void gfxBegin();
void gfxEnd();
uint16_t gfxColor(uint32_t c);
void gfxText(const String &s, int x, int y, uint32_t color, int size);

static const int W = 64, H = 32;
static inline void px(int x, int y, uint32_t c) {
  if (x >= 0 && x < W && y >= 0 && y < H) gfxPanel()->drawPixel(x, y, gfxColor(c));
}
static inline void fill(int x, int y, int w, int h, uint32_t c) {
  if (w > 0 && h > 0) gfxPanel()->fillRect(x, y, w, h, gfxColor(c));
}
static inline void txt(const String &s, int x, int y, uint32_t c) { gfxText(s, x, y, c, 1); }
static int txtW(const String &s) { return (int)foldAscii(s).length() * 6 - 1; }
static void txtR(const String &s, int xr, int y, uint32_t c) { txt(s, xr - txtW(s) + 1, y, c); }
static void txtC(const String &s, int y, uint32_t c) { txt(s, (W - txtW(s)) / 2, y, c); }

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
  h = fmodf(h < 0 ? h + 1 : h, 1.0f) * 6;
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
static void disc(float cx, float cy, float r, uint32_t c) {
  for (int y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++)
    for (int x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      if (dx * dx + dy * dy <= r * r) px(x, y, c);
    }
}
static uint32_t hash32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352d;
  x ^= x >> 15;
  x *= 0x846ca68b;
  x ^= x >> 16;
  return x;
}
static uint32_t rs = 0x9E3779B9;
static uint32_t rnd() {
  rs ^= rs << 13;
  rs ^= rs >> 17;
  rs ^= rs << 5;
  return rs;
}
static float fr() { return (rnd() & 0xFFFFFF) / 16777216.0f; }

static String hm(const struct tm &t, bool suffix = false) {
  char b[12];
  if (CFG.twelveHour) {
    int h = t.tm_hour % 12;
    snprintf(b, sizeof b, suffix ? "%d:%02d%c" : "%d:%02d", h ? h : 12, t.tm_min, t.tm_hour < 12 ? 'A' : 'P');
  } else {
    snprintf(b, sizeof b, "%02d:%02d", t.tm_hour, t.tm_min);
  }
  return b;
}

// ---------------------------------------------------------------- 3x5 font
static uint16_t glyph3(char ch) {
  // five rows of three bits, top row first (bit 2 = left column)
#define G(a, b, c, d, e) (uint16_t)((0##a << 12) | (0##b << 9) | (0##c << 6) | (0##d << 3) | 0##e)
  if (ch >= 'a' && ch <= 'z') ch -= 32;
  switch (ch) {
    case '0': return G(7, 5, 5, 5, 7);
    case '1': return G(2, 6, 2, 2, 7);
    case '2': return G(7, 1, 7, 4, 7);
    case '3': return G(7, 1, 7, 1, 7);
    case '4': return G(5, 5, 7, 1, 1);
    case '5': return G(7, 4, 7, 1, 7);
    case '6': return G(7, 4, 7, 5, 7);
    case '7': return G(7, 1, 1, 2, 2);
    case '8': return G(7, 5, 7, 5, 7);
    case '9': return G(7, 5, 7, 1, 7);
    case 'A': return G(2, 5, 7, 5, 5);
    case 'B': return G(6, 5, 6, 5, 6);
    case 'C': return G(3, 4, 4, 4, 3);
    case 'D': return G(6, 5, 5, 5, 6);
    case 'E': return G(7, 4, 6, 4, 7);
    case 'F': return G(7, 4, 6, 4, 4);
    case 'G': return G(3, 4, 5, 5, 3);
    case 'H': return G(5, 5, 7, 5, 5);
    case 'I': return G(7, 2, 2, 2, 7);
    case 'J': return G(1, 1, 1, 5, 2);
    case 'K': return G(5, 5, 6, 5, 5);
    case 'L': return G(4, 4, 4, 4, 7);
    case 'M': return G(5, 7, 7, 5, 5);
    case 'N': return G(6, 5, 5, 5, 5);
    case 'O': return G(2, 5, 5, 5, 2);
    case 'P': return G(6, 5, 6, 4, 4);
    case 'Q': return G(2, 5, 5, 6, 3);
    case 'R': return G(6, 5, 6, 5, 5);
    case 'S': return G(3, 4, 2, 1, 6);
    case 'T': return G(7, 2, 2, 2, 2);
    case 'U': return G(5, 5, 5, 5, 7);
    case 'V': return G(5, 5, 5, 5, 2);
    case 'W': return G(5, 5, 7, 7, 5);
    case 'X': return G(5, 5, 2, 5, 5);
    case 'Y': return G(5, 5, 2, 2, 2);
    case 'Z': return G(7, 1, 2, 4, 7);
    case ':': return G(0, 2, 0, 2, 0);
    case '.': return G(0, 0, 0, 0, 2);
    case ',': return G(0, 0, 0, 2, 4);
    case '-': return G(0, 0, 7, 0, 0);
    case '+': return G(0, 2, 7, 2, 0);
    case '/': return G(1, 1, 2, 4, 4);
    case '%': return G(5, 1, 2, 4, 5);
    case '@': return G(2, 5, 7, 4, 3);
    case '\'': return G(2, 2, 0, 0, 0);
    case '&': return G(2, 5, 2, 5, 3);
    case '#': return G(5, 7, 5, 7, 5);
    case '(': return G(2, 4, 4, 4, 2);
    case ')': return G(2, 1, 1, 1, 2);
    case '?': return G(7, 1, 2, 0, 2);
    case '!': return G(2, 2, 2, 0, 2);
    case '<': return G(1, 2, 4, 2, 1);
    case '>': return G(4, 2, 1, 2, 4);
    case '*': return G(0, 5, 2, 5, 0);
    default: return 0;
  }
#undef G
}
int text3Width(const String &s0) {
  String s = foldAscii(s0);
  return s.length() ? (int)s.length() * 4 - 1 : 0;
}
int text3(const String &s0, int x, int y, uint32_t c) {
  String s = foldAscii(s0);  // "HØJGAARD" -> "HOJGAARD"
  for (unsigned i = 0; i < s.length(); i++) {
    uint16_t g = glyph3(s[i]);
    for (int r = 0; r < 5; r++)
      for (int k = 0; k < 3; k++)
        if (g & (1 << ((4 - r) * 3 + (2 - k)))) px(x + i * 4 + k, y + r, c);
  }
  return text3Width(s);
}
static void text3R(const String &s, int xr, int y, uint32_t c) { text3(s, xr - text3Width(s) + 1, y, c); }
static void text3C(const String &s, int y, uint32_t c) { text3(s, (W - text3Width(s)) / 2, y, c); }
static String fit3(String s, int maxPx) {
  s = foldAscii(s);
  while (s.length() && text3Width(s) > maxPx) s.remove(s.length() - 1);
  return s;
}

// ================================================================ 1. binary clock
// BCD: one column per digit (HH MM SS), bits 8-4-2-1 from the top; dim dots are the zeros
void drawBinaryClock(time_t now) {
  struct tm t;
  localtime_r(&now, &t);
  int hr = t.tm_hour;
  if (CFG.twelveHour) hr = hr % 12 ? hr % 12 : 12;
  int digits[6] = {hr / 10, hr % 10, t.tm_min / 10, t.tm_min % 10, t.tm_sec / 10, t.tm_sec % 10};
  int bits[6] = {2, 4, 3, 4, 3, 4};
  static const uint32_t COL[3] = {0xFF5030, 0x30E060, 0x3090FF};
  gfxBegin();
  const int pitch = 5, x0 = 9;
  for (int i = 0; i < 6; i++) {
    int x = x0 + i * pitch + (i / 2) * 4;
    uint32_t on = COL[i / 2];
    for (int b = 0; b < 4; b++) {
      int y = 2 + (3 - b) * pitch;
      if (b >= bits[i]) continue;
      bool lit = digits[i] & (1 << b);
      fill(x, y, 4, 4, lit ? on : scale(on, 0.12f));
      if (lit) px(x, y, mix(on, 0xFFFFFF, 0.5f));  // a little shine
    }
  }
  char buf[12];
  snprintf(buf, sizeof buf, "%d%d %d%d %d%d", digits[0], digits[1], digits[2], digits[3], digits[4], digits[5]);
  for (int i = 0; i < 3; i++) {
    String d = String(buf).substring(i * 3, i * 3 + 2);
    text3(d, x0 + i * (2 * pitch + 4) + 1, 25, scale(COL[i], 0.8f));
  }
  gfxEnd();
}

// ================================================================ 2. sun-arc clock
static uint32_t skyFor(float alt, bool top) {
  // night -> deep blue, twilight -> purple/orange at the horizon, day -> blue
  if (alt < -12) return top ? 0x02030C : 0x060818;
  if (alt < -4) return mix(top ? 0x02030C : 0x060818, top ? 0x1A1040 : 0x6A2848, (alt + 12) / 8);
  if (alt < 4) return mix(top ? 0x1A1040 : 0x6A2848, top ? 0x2050A0 : 0xFF8040, (alt + 4) / 8);
  return mix(top ? 0x2050A0 : 0xFF8040, top ? 0x1E64C8 : 0x70B8F0, std::min(1.0f, (alt - 4) / 20));
}
void drawSunArcClock(time_t now, float lat, float lon) {
  struct tm t;
  localtime_r(&now, &t);
  bool located = lat != 0 || lon != 0;
  time_t rise = 0, set = 0;
  if (!located || !sunTimes(lat, lon, now, rise, set)) {  // no location (or polar): a generic 6 to 6 day
    struct tm m = t;
    m.tm_hour = 6, m.tm_min = 0, m.tm_sec = 0;
    rise = mktime(&m);
    set = rise + 12 * 3600;
  }
  float alt = located ? (float)sunAltitude(now, lat, lon)
                      : (now > rise && now < set ? 40 * sinf((float)(now - rise) / (set - rise) * 3.14159f) : -20);
  gfxBegin();
  for (int y = 0; y < 26; y++) fill(0, y, W, 1, mix(skyFor(alt, true), skyFor(alt, false), y / 25.0f));
  bool day = now >= rise && now < set;
  if (!day)  // stars
    for (int i = 0; i < 18; i++) {
      uint32_t h = hash32(i * 7919 + 17);
      if ((h >> 20) % 5 == (unsigned)((now / 3 + i) % 5)) continue;  // slow twinkle
      px(h % W, (h >> 8) % 20, alt < -8 ? 0x8080A0 : 0x404058);
    }
  // the arc: horizon at y 25, top of the arc at y 4
  const float cx = 31.5f, rx = 28, ry = 21, base = 25;
  for (int i = 0; i <= 56; i += 2) {
    float a = 3.14159f * i / 56;
    px((int)lroundf(cx - rx * cosf(a)), (int)lroundf(base - ry * sinf(a)), day ? 0x506080 : 0x283048);
  }
  float f;
  if (day) {
    f = (float)(now - rise) / std::max<long>(1, (long)(set - rise));
  } else {
    time_t s0 = now >= set ? set : set - 86400, r1 = now >= set ? rise + 86400 : rise;
    f = (float)(now - s0) / std::max<long>(1, (long)(r1 - s0));
  }
  f = std::max(0.0f, std::min(1.0f, f));
  float bx = cx - rx * cosf(3.14159f * f), by = base - ry * sinf(3.14159f * f);
  if (day) {
    disc(bx, by, 4.2f, scale(0xFFB000, 0.35f));  // glow
    disc(bx, by, 2.6f, 0xFFE040);
  } else {
    disc(bx, by, 3.0f, 0xE8E8F4);
    disc(bx + 1.6f, by - 0.6f, 2.4f, mix(skyFor(alt, true), skyFor(alt, false), by / 25.0f));  // crescent
  }
  // ground with the time on it
  fill(0, 26, W, 6, day ? 0x0E2A10 : 0x060E08);
  fill(0, 25, W, 1, day ? 0x2A6030 : 0x10281A);
  txtC(hm(t, false), 25, day ? 0xFFFFFF : 0xC0C8FF);
  // sunrise / sunset times in the corners while the sun is down near them
  struct tm rt, st;
  localtime_r(&rise, &rt);
  localtime_r(&set, &st);
  if (located) {
    text3(hm(rt), 0, 27, 0xFFB060);
    text3R(hm(st), 63, 27, 0xFF7050);
  }
  gfxEnd();
}

// ================================================================ 3. pixel city
void drawCityClock(time_t now, int frame) {
  struct tm t;
  localtime_r(&now, &t);
  float hour = t.tm_hour + t.tm_min / 60.0f;
  // sky by the clock
  float dayness = hour < 5 ? 0 : hour < 7 ? (hour - 5) / 2 : hour < 17 ? 1 : hour < 19.5f ? 1 - (hour - 17) / 2.5f : 0;
  bool dusk = (hour >= 17 && hour < 20) || (hour >= 5 && hour < 7.5f);
  uint32_t top = mix(0x03041A, 0x3070D0, dayness), bot = mix(0x0A0C30, dusk ? 0xF08050 : 0x90C8F0, dayness);
  gfxBegin();
  for (int y = 0; y < 30; y++) fill(0, y, W, 1, mix(top, bot, y / 29.0f));
  if (dayness < 0.3f)
    for (int i = 0; i < 22; i++) {
      uint32_t h = hash32(i * 104729 + 3);
      if ((h >> 24) % 7 == (unsigned)((frame / 20 + i) % 7)) continue;
      px(h % W, (h >> 8) % 14, scale(0xC0C0E0, 0.6f - dayness));
    }
  // buildings: fixed skyline, windows switch on and off with the hour
  float litChance = hour < 5 ? 0.12f : hour < 8 ? 0.35f : hour < 16 ? 0.06f : hour < 22 ? 0.6f : 0.3f;
  int x = 0, b = 0;
  while (x < W) {
    uint32_t h = hash32(b * 2654435761u + 99);
    int w = 5 + h % 6, ht = 9 + (h >> 4) % 13;
    if (b % 4 == 2) ht += 3;  // a few towers
    int y0 = 30 - ht;
    uint32_t wall = mix(0x14161E, 0x30343C, ((h >> 9) % 5) / 5.0f);
    if (dayness > 0.5f) wall = mix(wall, 0x5A606C, (dayness - 0.5f));
    fill(x, y0, w, ht, wall);
    if (b % 4 == 2) px(x + w / 2, y0 - 1, (frame / 15) % 2 ? 0xFF2020 : 0x400000);  // aircraft light
    for (int wy = y0 + 2; wy < 29; wy += 2)
      for (int wx = x + 1; wx < x + w - 1; wx += 2) {
        uint32_t wh = hash32((uint32_t)(wx * 131 + wy * 7 + b * 977) ^ (uint32_t)(t.tm_hour * 2654435761u) ^
                             (uint32_t)((now / 97 + wx) / 13));
        bool lit = (wh & 1023) < litChance * 1024;
        if (lit) px(wx, wy, ((wh >> 10) & 3) ? 0xFFD070 : 0xA0E0FF);
      }
    x += w + (h >> 12) % 2;
    b++;
  }
  // street and traffic
  fill(0, 30, W, 2, 0x101010);
  for (int c = 0; c < 4; c++) {
    int cx = (frame * (c % 2 ? -1 : 1) * (1 + c % 3) / 2 + c * 23) % (W + 20);
    if (cx < 0) cx += W + 20;
    cx -= 10;
    if (c % 2) {
      px(cx, 31, 0xFF2010), px(cx + 2, 31, 0xFF2010);
    } else {
      px(cx, 30, 0xFFFFE0), px(cx + 2, 30, 0xFFFFE0);
    }
  }
  // time in the sky
  String s = hm(t, false);
  txt(s, 2, 1, dayness > 0.6f ? 0xFFFFFF : 0xFFE0A0);
  gfxEnd();
}

// ================================================================ 4. world clock
static int64_t civilDays(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  return (int64_t)era * 146097 + (int64_t)(yoe * 365 + yoe / 4 - yoe / 100 + doy) - 719468;
}
// day of month of the nth Sunday (n = -1: the last one)
static int sundayOf(int y, int m, int n) {
  int dim = (int)(civilDays(y + (m == 12), m == 12 ? 1 : m + 1, 1) - civilDays(y, m, 1));
  int wd1 = (int)((civilDays(y, m, 1) + 4) % 7);  // 1970-01-01 was a Thursday
  if (wd1 < 0) wd1 += 7;
  int first = 1 + (7 - wd1) % 7;
  if (n > 0) return first + 7 * (n - 1);
  int d = first;
  while (d + 7 <= dim) d += 7;
  return d;
}
static time_t utcOf(int y, int m, int d, int hour) { return (time_t)(civilDays(y, m, d) * 86400 + hour * 3600); }

time_t worldLocal(time_t utc, const WorldCity &c, bool *dstOut) {
  time_t std = utc + c.utcMin * 60;
  int64_t days = (int64_t)std / 86400 - (std < 0 && std % 86400);
  // year of the local standard time
  int y = 1970 + (int)(days / 365.2425);
  while (civilDays(y, 1, 1) > days) y--;
  while (civilDays(y + 1, 1, 1) <= days) y++;
  bool dst = false;
  if (c.dst == "us") {  // 2nd Sunday March 02:00 local standard -> 1st Sunday November 02:00 local daylight
    time_t on = utcOf(y, 3, sundayOf(y, 3, 2), 2), off = utcOf(y, 11, sundayOf(y, 11, 1), 1);
    dst = std >= on && std < off;
  } else if (c.dst == "eu") {  // last Sunday March 01:00 UTC -> last Sunday October 01:00 UTC
    dst = utc >= utcOf(y, 3, sundayOf(y, 3, -1), 1) && utc < utcOf(y, 10, sundayOf(y, 10, -1), 1);
  } else if (c.dst == "au") {  // southern: 1st Sunday October 02:00 -> 1st Sunday April 03:00 (daylight)
    time_t off = utcOf(y, 4, sundayOf(y, 4, 1), 2), on = utcOf(y, 10, sundayOf(y, 10, 1), 2);
    dst = std < off || std >= on;
  }
  if (dstOut) *dstOut = dst;
  return std + (dst ? 3600 : 0);
}

void drawWorldClock(time_t now) {
  gfxBegin();
  int n = std::min<int>(4, CFG.worldClocks.size());
  struct tm home;
  localtime_r(&now, &home);
  for (int i = 0; i < n; i++) {
    const WorldCity &c = CFG.worldClocks[i];
    time_t lt = worldLocal(now, c);
    struct tm t;
    gmtime_r(&lt, &t);
    int y = i * 8 + (n < 4 ? (32 - n * 8) / 2 : 0);
    bool day = t.tm_hour >= 6 && t.tm_hour < 18;
    uint32_t nameCol = 0xA0A8B8;
    // a day ahead or behind your own date: tint the name
    int dd = (t.tm_year * 400 + t.tm_yday) - (home.tm_year * 400 + home.tm_yday);
    if (dd > 0) nameCol = 0x60D0FF;
    if (dd < 0) nameCol = 0xFFA060;
    String name = c.name;
    name.toUpperCase();
    text3(fit3(name, 26), 0, y + 1, nameCol);
    if (day) {
      fill(28, y + 2, 3, 3, 0xFFC000);
    } else {
      fill(28, y + 2, 3, 3, 0x7080FF);
      px(30, y + 2, 0), px(30, y + 3, 0);  // crescent
    }
    char b[12];
    int h = t.tm_hour;
    if (CFG.twelveHour) snprintf(b, sizeof b, "%d:%02d", h % 12 ? h % 12 : 12, t.tm_min);
    else snprintf(b, sizeof b, "%02d:%02d", h, t.tm_min);
    txtR(b, CFG.twelveHour ? 59 : 63, y, day ? 0xFFFFFF : 0xB0B8FF);
    if (CFG.twelveHour) text3(h < 12 ? "A" : "P", 61, y + 2, 0x707070);
  }
  gfxEnd();
}

// ================================================================ 5. sand clock
// minutes pile up as sand (24 grains a minute); at the top of the hour the floor opens and it drains
static uint8_t sand[H][W];  // 0 empty, else a colour shade 1..255
static int sandCount = 0, sandDrain = 0;
void drawSandClock(time_t now, float gx, float gy, bool restart) {
  struct tm t;
  localtime_r(&now, &t);
  int target = t.tm_min * 24 + t.tm_sec * 24 / 60;
  if (restart) {  // settle the pile straight away instead of pouring it
    memset(sand, 0, sizeof sand);
    sandCount = 0;
    sandDrain = 0;
    for (int i = 0; i < target; i++) {
      int row = H - 1 - i / W, col = i % W;
      sand[row][col] = 1 + (hash32(i) % 254);
    }
    sandCount = target;
  }
  if (target + 100 < sandCount) sandDrain = 1;  // a new hour: drain
  if (sandDrain && sandCount == 0) sandDrain = 0;
  // pour from the spout while behind the time
  if (!sandDrain && sandCount < target) {
    int x = 31 + (int)(rnd() % 3) - 1;
    if (!sand[0][x]) sand[0][x] = 1 + rnd() % 254, sandCount++;
  }
  // gravity: one of 8 directions from the accelerometer (straight down without one)
  int dx = 0, dy = 1;
  if (fabsf(gx) > 0.15f || fabsf(gy) > 0.15f) {
    float a = atan2f(gy, gx);
    int o = ((int)lroundf(a / (3.14159f / 4)) + 8) % 8;
    static const int DX[8] = {1, 1, 0, -1, -1, -1, 0, 1}, DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    dx = DX[o], dy = DY[o];
  }
  // the side-steps when the way is blocked (the two neighbours of the gravity direction)
  int ax = dx - dy, ay = dy + dx, bx = dx + dy, by = dy - dx;
  auto norm = [](int &a, int &b) {
    a = a > 0 ? 1 : a < 0 ? -1 : 0;
    b = b > 0 ? 1 : b < 0 ? -1 : 0;
  };
  norm(ax, ay);
  norm(bx, by);
  // sweep against gravity so each grain moves at most once a frame
  bool flipX = dx > 0, flipY = dy > 0;
  for (int yi = 0; yi < H; yi++) {
    int y = flipY ? H - 1 - yi : yi;
    for (int xi = 0; xi < W; xi++) {
      int x = flipX ? W - 1 - xi : xi;
      if (!sand[y][x]) continue;
      bool first = rnd() & 1;
      // a diagonal pull: a surface grain may also slide sideways (across the pull), so the pile levels out
      // across it instead of freezing at 45 degrees
      bool diag = dx && dy, slide = diag && (rnd() & 7) == 0;
      int px1 = first ? dy : -dy, py1 = first ? -dx : dx;
      const int cand[4][2] = {{dx, dy}, {first ? ax : bx, first ? ay : by}, {first ? bx : ax, first ? by : ay},
                              {slide ? px1 : 0, slide ? py1 : 0}};
      for (auto &c : cand) {
        if (!c[0] && !c[1]) continue;
        int nx = x + c[0], ny = y + c[1];
        if (sandDrain && ny >= H) {  // through the open floor
          sand[y][x] = 0;
          sandCount--;
          break;
        }
        if (nx < 0 || nx >= W || ny < 0 || ny >= H || sand[ny][nx]) continue;
        sand[ny][nx] = sand[y][x];
        sand[y][x] = 0;
        break;
      }
    }
  }
  gfxBegin();
  float hue = 0.09f + 0.03f * sinf(t.tm_hour * 0.7f);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      if (sand[y][x]) px(x, y, hsv(hue + (sand[y][x] % 16) * 0.002f, 0.55f + (sand[y][x] % 7) * 0.03f, 0.55f + (sand[y][x] % 5) * 0.08f));
  // the time, outlined so it reads over the sand
  String s = hm(t, false);
  for (int ox = -1; ox <= 1; ox++)
    for (int oy = -1; oy <= 1; oy++)
      if (ox || oy) txt(s, 2 + ox, 1 + oy, 0x000000);
  txt(s, 2, 1, 0xFFFFFF);
  gfxEnd();
}

// ================================================================ 10. win probability
// team colours bright enough to read on black (navy and maroon get lifted toward white)
static uint32_t readable(uint32_t c) {
  int l = luminance(c);
  return l >= 90 ? c : mix(c, 0xFFFFFF, (90 - l) / 140.0f + 0.15f);
}
static uint32_t teamCol(const Team &t) {
  uint32_t c = t.color;
  if (luminance(c) < 25) c = t.alt;
  if (luminance(c) < 25) c = 0xA5ACAF;
  return readable(c);
}
void drawWinProb(const WinProb &wp, time_t now, bool showStatus) {
  const Game &g = wp.game;
  uint32_t ha = teamCol(g.home), aw = teamCol(g.away);
  if (ha == aw) aw = readable(teamAlt(g.away));
  gfxBegin();
  txt(g.away.abbr, 0, 0, aw);
  txtR(g.home.abbr, 63, 0, ha);
  int last = wp.home.empty() ? 50 : wp.home.back();
  if (showStatus || wp.home.empty()) {
    String st = statusText(g, now);
    text3C(fit3(st, 28), 2, 0xA0A0A0);
  } else {
    bool homeLeads = last >= 50;
    String pct = String(homeLeads ? last : 100 - last) + "%";
    text3C(pct, 2, homeLeads ? ha : aw);
  }
  // graph: 50% line at y 20, home above, away below
  const int top = 9, bot = 31, mid = 20;
  for (int x = 0; x < W; x += 2) px(x, mid, 0x303030);
  int n = wp.home.size();
  int prevY = -1;
  for (int x = 0; x < W && n; x++) {
    int i = n == 1 ? 0 : (int)((long)x * (n - 1) / (W - 1));
    int v = wp.home[i];
    int y = mid - (int)lroundf((v - 50) * (mid - top) / 50.0f);
    if (y > bot) y = bot;
    uint32_t c = v >= 50 ? ha : aw;
    for (int yy = std::min(y, mid); yy <= std::max(y, mid); yy++) px(x, yy, scale(c, 0.3f));
    if (prevY >= 0) line(x - 1, prevY, x, y, c);
    else px(x, y, c);
    prevY = y;
  }
  // current score in the corners of the graph
  text3(String(g.away.score), 0, 26, scale(aw, 0.9f));
  text3R(String(g.home.score), 63, top + 1, scale(ha, 0.9f));
  gfxEnd();
}

// ================================================================ 12. golf
int golfPages(const GolfBoard &b) { return b.rows.size() <= 3 ? 1 : 1 + (int)(b.rows.size() - 3 + 3) / 4; }
static String lastName(const String &n) {
  int dot = n.lastIndexOf(". ");
  if (dot >= 0) return n.substring(dot + 2);
  int sp = n.lastIndexOf(' ');
  return sp >= 0 ? n.substring(sp + 1) : n;
}
static void golfRow(const GolfRow &r, int y, bool leader) {
  String pos = r.pos.length() > 2 && r.pos[0] == 'T' ? r.pos.substring(1) : r.pos;
  text3(pos, 0, y, 0x909090);
  text3(fit3(lastName(r.name), 38), 12, y, leader ? 0xFFD700 : 0xE0E0E0);
  uint32_t sc = r.score.startsWith("-") ? 0xFF4040 : r.score == "E" ? 0xFFFFFF : 0x60A0FF;  // red = under par
  text3R(r.score, 63, y, sc);
}
void drawGolf(const GolfBoard &b, int page) {
  gfxBegin();
  if (page == 0) {
    text3(fit3(b.event, 64), 0, 0, 0x30C050);
    String d = b.detail;
    d.toUpperCase();
    d.replace("ROUND ", "R");
    d.replace(" - ", " ");
    d.replace("IN PROGRESS", "LIVE");
    text3(fit3(d, 64), 0, 7, 0x707070);
    for (int i = 0; i < 3 && i < (int)b.rows.size(); i++) golfRow(b.rows[i], 14 + i * 6, i == 0);
  } else {
    int start = 3 + (page - 1) * 4;
    for (int i = 0; i < 4 && start + i < (int)b.rows.size(); i++) golfRow(b.rows[start + i], 2 + i * 8, false);
  }
  gfxEnd();
}

// ================================================================ 13. next game
void drawNextGame(const NextGame &n, time_t now, bool showLine) {
  gfxBegin();
  // "LAL @ SA" (or "LAL VS SA" at home) in the teams' colours
  String sep = n.home ? " VS " : " @ ";
  int w = txtW(n.us + sep + n.them), x = (W - w) / 2;
  txt(n.us, x, 0, readable(n.usColor));
  x += txtW(n.us) + 1;
  txt(sep, x, 0, 0x707070);
  x += txtW(sep) + 1;
  txt(n.them, x, 0, readable(n.themColor));
  txtC(fmtStart(n.start, now), 9, 0xFFFFFF);
  long d = (long)(n.start - now);
  String cd = d <= 0 ? "STARTING" : d < 3600 ? "IN " + String(d / 60) + "M"
                                  : d < 86400 ? "IN " + String(d / 3600) + "H " + String((d % 3600) / 60) + "M"
                                              : "IN " + String(d / 86400) + "D " + String((d % 86400) / 3600) + "H";
  text3C(cd, 18, d < 3 * 3600 ? 0xFFC000 : 0x80C0FF);
  String bottom;
  if (showLine && n.line.length()) {
    bottom = n.line;
    if (n.overUnder.length()) bottom += " O/U " + n.overUnder;
  } else if (n.tv.length()) {
    bottom = "TV " + n.tv;
  } else if (n.line.length()) {
    bottom = n.line;
  }
  bottom.toUpperCase();
  text3C(fit3(bottom, 64), 25, 0xA0A0A0);
  gfxEnd();
}

// ================================================================ 14. top performers
void drawHighlight(const Highlight &h0, const String &result) {
  Highlight h = h0;
  h.color = readable(h.color);
  gfxBegin();
  txt(h.team, 0, 0, h.color);
  text3R(result, 63, 1, result.startsWith("W") ? 0x40FF60 : result.startsWith("L") ? 0xFF5050 : 0xA0A0A0);
  fill(0, 8, W, 1, scale(h.color, 0.4f));
  text3(fit3(h.who, 64), 0, 12, 0xFFFFFF);
  // the stat line, wrapped onto two rows of the small font
  String s = h.what, a = s, b;
  if (text3Width(s) > 64) {
    int cut = s.lastIndexOf(' ', 15);
    if (cut < 0) cut = 15;
    a = s.substring(0, cut);
    b = s.substring(cut + 1);
  }
  text3(fit3(a, 64), 0, 19, 0xFFC000);
  text3(fit3(b, 64), 0, 26, 0xFFC000);
  gfxEnd();
}

// ================================================================ fantasy points
static String pts1(float p) {
  if (fabsf(p) >= 100) return String((int)lroundf(p));
  char b[12];
  snprintf(b, sizeof b, "%.1f", p);
  return b;
}
void drawFantasyTotals(const FptsBoard &b, int frame) {
  gfxBegin();
  bool lead = b.me >= b.opp;
  uint32_t meC = b.anyStarted ? (lead ? 0x40FF60 : 0xFF6050) : 0xFFFFFF;
  uint32_t opC = b.anyStarted ? (!lead ? 0x40FF60 : 0xFF6050) : 0xFFFFFF;
  String me = b.meLabel, op = b.oppLabel;
  me.toUpperCase();
  op.toUpperCase();
  txt(me.substring(0, 6), 0, 0, 0xFFC000);
  txtR(pts1(b.me), 63, 0, meC);
  txt(op.substring(0, 6), 0, 9, 0x80A0C0);
  txtR(pts1(b.opp), 63, 9, opC);
  // share of the points so far
  float tot = b.me + b.opp;
  int split = tot > 0 ? (int)lroundf(W * std::max(0.0f, b.me) / std::max(0.01f, std::max(0.0f, b.me) + std::max(0.0f, b.opp))) : W / 2;
  fill(0, 19, split, 3, 0x30B040);
  fill(split, 19, W - split, 3, 0xB03030);
  if (b.anyLive) px(split, 18, (frame / 4) % 2 ? 0xFFFFFF : 0x606060), px(split, 22, (frame / 4) % 2 ? 0xFFFFFF : 0x606060);
  int live = 0, done = 0, total = 0;
  for (auto &l : b.lines) {
    if (l.state == 'n') continue;
    total++;
    live += l.state == 'i';
    done += l.state == 'f';
  }
  String st = live ? String(live) + " PLAYING" : done == total && total ? "ALL FINAL" : b.anyStarted ? "BETWEEN GAMES" : "NOT STARTED";
  if (b.week) st = "WK" + String(b.week) + " " + st;
  text3(fit3(st, 40), 0, 26, live ? 0x40FF60 : 0x808080);
  float diff = b.me - b.opp;
  if (b.anyStarted) text3R((diff >= 0 ? "+" : "") + pts1(diff), 63, 26, diff >= 0 ? 0x40FF60 : 0xFF6050);
  gfxEnd();
}
int fantasyPages(const FptsBoard &b, int side) {
  int n = 0;
  for (auto &l : b.lines) n += l.side == side;
  return (n + 3) / 4;
}
static uint32_t posColor(const String &p) {
  if (p == "QB") return 0xFF4060;
  if (p == "RB") return 0x30E080;
  if (p == "WR") return 0x40A0FF;
  if (p == "TE") return 0xFF9020;
  if (p == "K" || p == "PK") return 0xB070FF;
  if (p == "DST") return 0xA0A0A0;
  return 0xFFFFFF;
}
void drawFantasyPage(const FptsBoard &b, int side, int page, int frame) {
  gfxBegin();
  String label = side ? b.oppLabel : b.meLabel;
  label.toUpperCase();
  text3(fit3(label, 44), 0, 0, side ? 0x80A0C0 : 0xFFC000);
  text3R(pts1(side ? b.opp : b.me), 63, 0, 0xFFFFFF);
  fill(0, 6, W, 1, 0x282828);
  int idx = 0, row = 0;
  for (auto &l : b.lines) {
    if (l.side != side) continue;
    if (idx++ / 4 != page) continue;
    int y = 8 + row * 6;
    fill(0, y, 2, 5, posColor(l.pos));
    String name = l.pos == "DST" ? l.team + " D/ST" : lastName(l.name);
    name.toUpperCase();
    // white = playing, green = on the field now, grey = done, dark = not started, dim red = no game
    uint32_t c = l.state == 'i' ? (l.hasBall ? ((frame / 6) % 4 ? 0x40FF60 : 0x20A040) : 0xFFFFFF)
                 : l.state == 'f' ? 0x909090 : l.state == 'p' ? 0x505050 : 0x602020;
    text3(fit3(name, 42), 4, y, c);
    text3R(l.state == 'p' || l.state == 'n' ? String("-") : pts1(l.pts), 63, y, l.state == 'i' ? 0xFFFFFF : 0xB0B0B0);
    row++;
  }
  gfxEnd();
}

// ================================================================ 15. sound visualizer
void drawVisualizer(const uint8_t *bands, int n, bool mic, int frame) {
  static float peak[32];
  static int hold[32];
  gfxBegin();
  if (!mic) {
    txtC("NO MIC", 8, 0xFF6040);
    text3C("MICROPHONES NOT FOUND", 20, 0x707070);
    gfxEnd();
    return;
  }
  n = std::min(n, 16);
  int bw = W / n;
  for (int i = 0; i < n; i++) {
    float h = bands[i] / 255.0f * 31;
    if (h > peak[i]) peak[i] = h, hold[i] = 12;
    else if (hold[i] > 0) hold[i]--;
    else peak[i] = std::max(0.0f, peak[i] - 0.6f);
    int top = 31 - (int)h;
    for (int y = 31; y > top; y--) {
      float k = (31 - y) / 31.0f;
      uint32_t c = hsv(0.36f - 0.36f * k + 0.02f * sinf(frame * 0.05f + i * 0.3f), 1, 0.35f + 0.65f * k);
      fill(i * bw, y, bw - 1, 1, c);
    }
    int py = 31 - (int)peak[i];
    if (peak[i] > 0.5f) fill(i * bw, py, bw - 1, 1, 0xFFFFFF);
  }
  gfxEnd();
}

// ================================================================ 17. Game of Life
static uint8_t life[H][W], lifeNext[H][W];  // 0 dead, else age (capped)
static uint32_t lifeHist[12];
static int lifeGen = 0, lifeStale = 0;
static void lifeSeed(uint32_t seed) {
  rs = seed ? seed : 1;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) life[y][x] = fr() < 0.3f ? 1 : 0;
  lifeGen = 0;
  lifeStale = 0;
  memset(lifeHist, 0, sizeof lifeHist);
}
static void lifeGlider(int x, int y) {
  static const int G[5][2] = {{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}};
  bool flipX = rnd() & 1, flipY = rnd() & 1;
  for (auto &g : G) {
    int gx = flipX ? 2 - g[0] : g[0], gy = flipY ? 2 - g[1] : g[1];
    life[(y + gy) % H][(x + gx) % W] = 1;
  }
}
void drawLife(int frame, bool restart, bool poke) {
  if (restart) lifeSeed((uint32_t)time(nullptr) ^ 0xA5A5A5A5u);
  if (poke) lifeGlider(rnd() % W, rnd() % H);
  if (frame % 3 == 0) {  // ~13 generations a second at 25 fps
    uint32_t h = 2166136261u;
    int pop = 0;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        int nb = 0;
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++)
            if ((dx || dy) && life[(y + dy + H) % H][(x + dx + W) % W]) nb++;
        uint8_t a = life[y][x];
        uint8_t v = (a && (nb == 2 || nb == 3)) ? (uint8_t)std::min(255, a + 1) : (!a && nb == 3) ? 1 : 0;
        lifeNext[y][x] = v;
        if (v) {
          pop++;
          h = (h ^ (uint32_t)(y * W + x)) * 16777619u;
        }
      }
    memcpy(life, lifeNext, sizeof life);
    lifeGen++;
    // stuck in a loop (still life, blinkers, a lone glider) or nearly empty: start over
    bool repeat = false;
    for (uint32_t o : lifeHist) repeat |= o == h;
    memmove(lifeHist + 1, lifeHist, sizeof(lifeHist) - sizeof(uint32_t));
    lifeHist[0] = h;
    lifeStale = repeat ? lifeStale + 1 : 0;
    if (lifeStale > 40 || pop < 6 || lifeGen > 3000) lifeSeed(rnd());
  }
  gfxBegin();
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint8_t a = life[y][x];
      if (!a) continue;
      // newborns are bright cyan, elders cool to violet
      float k = std::min(1.0f, (a - 1) / 30.0f);
      px(x, y, a == 1 ? 0xC0FFFF : hsv(0.5f + 0.28f * k, 0.8f, 0.9f - 0.45f * k));
    }
  gfxEnd();
}

// ================================================================ 18. planes overhead
static void arrow(int cx, int cy, float deg, uint32_t c) {
  float a = deg * 3.14159f / 180;
  float dx = sinf(a), dy = -cosf(a);
  line((int)lroundf(cx - 2 * dx), (int)lroundf(cy - 2 * dy), (int)lroundf(cx + 2 * dx), (int)lroundf(cy + 2 * dy), c);
  px((int)lroundf(cx + 2 * dx - 1.2f * dy - dx), (int)lroundf(cy + 2 * dy + 1.2f * dx - dy), c);
  px((int)lroundf(cx + 2 * dx + 1.2f * dy - dx), (int)lroundf(cy + 2 * dy - 1.2f * dx - dy), c);
}
void drawFlights(const std::vector<Flight> &f, int sel, int radiusKm, int frame) {
  gfxBegin();
  const float cx = 13.5f, cy = 15.5f, R = 13.5f;
  // radar: rings, cross, sweep
  for (int a = 0; a < 360; a += 6) {
    float r = a * 3.14159f / 180;
    px((int)lroundf(cx + R * cosf(r)), (int)lroundf(cy + R * sinf(r)), 0x0C3A18);
    if (a % 12 == 0) px((int)lroundf(cx + R / 2 * cosf(r)), (int)lroundf(cy + R / 2 * sinf(r)), 0x082810);
  }
  float sweep = fmodf(frame * 0.09f, 6.2832f);
  for (int k = 0; k < 6; k++) {
    float a = sweep - k * 0.12f;
    line((int)cx, (int)cy, (int)lroundf(cx + R * sinf(a)), (int)lroundf(cy - R * cosf(a)), scale(0x30FF60, 0.5f - k * 0.08f));
  }
  px((int)cx, (int)cy, 0xFFFFFF);
  for (int i = (int)f.size() - 1; i >= 0; i--) {
    float r = std::min(1.0f, f[i].distKm / std::max(1, radiusKm)) * R, b = f[i].bearing * 3.14159f / 180;
    int x = (int)lroundf(cx + r * sinf(b)), y = (int)lroundf(cy - r * cosf(b));
    uint32_t c = i == sel ? ((frame / 5) % 2 ? 0xFFE040 : 0xFF9000) : 0x40FF70;
    px(x, y, c);
    if (i == sel) px(x + 1, y, c), px(x, y + 1, c), px(x + 1, y + 1, c);
  }
  // details of the selected plane
  const int tx = 30;
  if (f.empty()) {
    text3("NO PLANES", tx, 8, 0x808080);
    text3("WITHIN " + String(radiusKm) + "KM", tx, 15, 0x505050);
  } else {
    const Flight &p = f[sel % f.size()];
    text3(fit3(p.callsign, 34), tx, 1, 0xFFE040);
    String alt = p.altFt >= 10000 ? String(p.altFt / 1000) + "," + String((p.altFt % 1000) / 100) + "00FT"
                                  : String(p.altFt) + "FT";
    text3(fit3(alt, 34), tx, 8, 0xFFFFFF);
    if (p.climbFpm > 300) text3("+", 60, 8, 0x40FF60);
    else if (p.climbFpm < -300) text3("-", 60, 8, 0xFF6040);
    text3(String(p.speedKt) + "KT", tx, 15, 0xA0C0FF);
    arrow(58, 17, (float)p.heading, 0xA0C0FF);
    text3(String((int)lroundf(p.distKm)) + "KM " + compass8(p.bearing), tx, 22, 0x80FF90);
  }
  text3R(String(f.size()), 63, 27, 0x305030);
  gfxEnd();
}
