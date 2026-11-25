// gfxutil.h - small drawing helpers shared by the batch 5 screens (render5*.cpp, pet.cpp).
// Everything is inline/static so unused helpers cost nothing. Coordinates are in the panel's current rotation:
// 64x32 in landscape, 32x64 when the caller has turned the panel for a portrait screen (the panel clips).
#pragma once
#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <algorithm>
#include <cmath>

// exported by render.cpp
MatrixPanel_I2S_DMA *gfxPanel();
void gfxBegin();       // start a frame (clears the back buffer)
void gfxEnd();         // show the frame
uint16_t gfxColor(uint32_t c);
void gfxText(const String &s, int x, int y, uint32_t color, int size);   // 6x8 font (5x7 glyphs), size 1 = normal
String foldAscii(const String &s);                                        // accents -> plain letters
// exported by render3.cpp: a compact 3x5 font (digits, capitals, a few symbols); returns the width drawn
int text3(const String &s, int x, int y, uint32_t color);
int text3Width(const String &s);

namespace gu {
inline int sw() { return gfxPanel()->width(); }
inline int sh() { return gfxPanel()->height(); }
inline void px(int x, int y, uint32_t c) {
  if (x >= 0 && y >= 0 && x < sw() && y < sh()) gfxPanel()->drawPixel(x, y, gfxColor(c));
}
inline void fill(int x, int y, int w, int h, uint32_t c) {
  if (w > 0 && h > 0) gfxPanel()->fillRect(x, y, w, h, gfxColor(c));
}
inline void rect(int x, int y, int w, int h, uint32_t c) {
  if (w > 0 && h > 0) gfxPanel()->drawRect(x, y, w, h, gfxColor(c));
}
inline void txt(const String &s, int x, int y, uint32_t c) { gfxText(s, x, y, c, 1); }
inline int txtW(const String &s) { int n = (int)foldAscii(s).length(); return n ? n * 6 - 1 : 0; }
inline void txtR(const String &s, int xr, int y, uint32_t c) { txt(s, xr - txtW(s) + 1, y, c); }
inline void txtC(const String &s, int y, uint32_t c) { txt(s, (sw() - txtW(s)) / 2, y, c); }
inline void t3C(const String &s, int y, uint32_t c) { text3(s, (sw() - text3Width(s)) / 2, y, c); }
inline void t3R(const String &s, int xr, int y, uint32_t c) { text3(s, xr - text3Width(s) + 1, y, c); }

inline uint32_t rgb(int r, int g, int b) {
  return ((uint32_t)std::max(0, std::min(255, r)) << 16) | ((uint32_t)std::max(0, std::min(255, g)) << 8) |
         (uint32_t)std::max(0, std::min(255, b));
}
inline uint32_t scale(uint32_t c, float k) {
  k = std::max(0.0f, std::min(1.0f, k));
  return ((uint32_t)(((c >> 16) & 255) * k) << 16) | ((uint32_t)(((c >> 8) & 255) * k) << 8) | (uint32_t)((c & 255) * k);
}
inline uint32_t mix(uint32_t a, uint32_t b, float t) {
  t = std::max(0.0f, std::min(1.0f, t));
  auto ch = [&](int s) { return (uint32_t)(((a >> s) & 255) * (1 - t) + ((b >> s) & 255) * t) << s; };
  return ch(16) | ch(8) | ch(0);
}
inline uint32_t hsv(float h, float s, float v) {
  h = fmodf(h < 0 ? h + 1 : h, 1.0f) * 6;
  int i = (int)h;
  float f = h - i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f)), r, g, b;
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
inline void line(int x0, int y0, int x1, int y1, uint32_t c) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for (int n = 0; n < 256; n++) {
    px(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) err += dy, x0 += sx;
    if (e2 <= dx) err += dx, y0 += sy;
  }
}
inline void disc(float cx, float cy, float r, uint32_t c) {
  for (int y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++)
    for (int x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      if (dx * dx + dy * dy <= r * r) px(x, y, c);
    }
}
inline void ring(float cx, float cy, float r, uint32_t c) {
  for (int a = 0; a < 360; a += 3) {
    float t = a * (float)M_PI / 180;
    px((int)floorf(cx + r * cosf(t)), (int)floorf(cy + r * sinf(t)), c);
  }
}
inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
  return x;
}
}  // namespace gu
