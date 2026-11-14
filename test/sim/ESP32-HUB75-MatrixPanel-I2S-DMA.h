// Desktop stand-in for the HUB75 DMA driver: draws into a 64x32 RGB buffer so screens can be previewed.
#pragma once
#include "Arduino.h"
#include <cstring>
#include "glcdfont.c"

struct HUB75_I2S_CFG {
  enum shift_driver { SHIFTREG, FM6124, FM6126A, ICN2038S, MBI5124 };
  struct i2s_pins { int r1, g1, b1, r2, g2, b2, a, b, c, d, e, lat, oe, clk; };
  HUB75_I2S_CFG(int, int, int, i2s_pins) {}
  shift_driver driver = SHIFTREG;
  bool clkphase = false, double_buff = false;
};

class MatrixPanel_I2S_DMA {
 public:
  uint32_t px[32][64];      // what's being drawn
  uint32_t shown[32][64];   // last flipped frame
  int cx = 0, cy = 0, ts = 1;
  uint32_t tc = 0xFFFFFF;
  explicit MatrixPanel_I2S_DMA(const HUB75_I2S_CFG &) { memset(px, 0, sizeof px); memset(shown, 0, sizeof shown); }
  bool begin() { return true; }
  // real RGB565, like the panel driver; expanded back to 24-bit when drawn
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
  static uint32_t rgb(uint32_t c) {
    uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return ((r * 255 / 31) << 16) | ((g * 255 / 63) << 8) | (b * 255 / 31);
  }
  void setBrightness8(uint8_t) {}
  void setTextWrap(bool) {}
  void clearScreen() { memset(px, 0, sizeof px); }
  void fillScreen(uint32_t c) { fillRect(0, 0, width(), height(), c); }
  void flipDMABuffer() { memcpy(shown, px, sizeof px); }
  int rotation = 0;  // same mapping as the real driver's transform()
  void setRotation(int r) { rotation = r & 3; }
  int width() const { return rotation & 1 ? 32 : 64; }
  int height() const { return rotation & 1 ? 64 : 32; }
  void drawPixel(int x, int y, uint32_t c) {
    int t;
    switch (rotation) {
      case 1: t = 63 - y; y = x; x = t; break;
      case 2: x = 63 - x; y = 31 - y; break;
      case 3: t = y; y = 31 - x; x = t; break;
    }
    if (x >= 0 && x < 64 && y >= 0 && y < 32) px[y][x] = rgb(c);
  }
  void fillRect(int x, int y, int w, int h, uint32_t c) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) drawPixel(i, j, c);
  }
  void drawRect(int x, int y, int w, int h, uint32_t c) {
    for (int i = x; i < x + w; i++) { drawPixel(i, y, c); drawPixel(i, y + h - 1, c); }
    for (int j = y; j < y + h; j++) { drawPixel(x, j, c); drawPixel(x + w - 1, j, c); }
  }
  void setTextSize(int s) { ts = s; }
  void setTextColor(uint32_t c) { tc = c; }
  void setCursor(int x, int y) { cx = x; cy = y; }
  void print(const String &s) {
    for (unsigned k = 0; k < s.length(); k++) {
      unsigned char ch = s[k];
      for (int i = 0; i < 5; i++) {
        unsigned char line = font[ch * 5 + i];
        for (int j = 0; j < 8; j++, line >>= 1)
          if (line & 1) fillRect(cx + i * ts, cy + j * ts, ts, ts, tc);
      }
      cx += 6 * ts;
    }
  }
};
inline void delay(unsigned long) {}
