#include "images.h"
#include <AnimatedGIF.h>
#include <JPEGDEC.h>
#include <PNGdec.h>
#include <SD.h>
#include <cmath>
#include <new>
#include <algorithm>
#include <vector>
#include "render.h"
#include "tronbyt_assets.h"  // YULE_GIF: Tidbyt's Yule Log (tronbyt/apps, Apache-2.0; video by Pexels)

static const int W = 64, H = 32;
static std::vector<String> files;
static size_t order = 0;
static uint16_t *canvas = nullptr;  // 64x32 RGB565, the picture being built
static int offX = 0, offY = 0;      // where the image's (0,0) lands on the panel

static bool wanted(const String &n) {
  String s = n;
  s.toLowerCase();
  if (s.startsWith(".") || s.indexOf("/.") >= 0) return false;  // macOS "._name" shadow files
  return s.endsWith(".gif") || s.endsWith(".png") || s.endsWith(".jpg") || s.endsWith(".jpeg");
}

int imagesScan() {
  files.clear();
  File dir = SD.open("/images");
  if (dir && dir.isDirectory()) {
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      String name = f.name();
      if (!f.isDirectory() && wanted(name)) files.push_back(name.startsWith("/") ? name : "/images/" + name);
      f.close();
    }
  }
  // shuffle (Fisher-Yates)
  for (int i = (int)files.size() - 1; i > 0; i--) std::swap(files[i], files[esp_random() % (i + 1)]);
  order = 0;
  Serial.printf("[images] %d in /images\n", (int)files.size());
  return files.size();
}

int imageCount() { return files.size(); }

static void put(int x, int y, uint16_t c) {
  x += offX;
  y += offY;
  if (x >= 0 && x < W && y >= 0 && y < H) canvas[y * W + x] = c;
}

// ---- GIF: draw one line of a frame into the canvas (transparent pixels keep what's under them)
static void gifDraw(GIFDRAW *d) {
  int y = d->iY + d->y;
  for (int i = 0; i < d->iWidth; i++) {
    uint8_t idx = d->pPixels[i];
    if (d->ucHasTransparency && idx == d->ucTransparent) continue;
    put(d->iX + i, y, d->pPalette[idx]);
  }
}

// ---- PNG: one line, converted to RGB565 on a black background
static uint16_t pngLine[1024];
static PNG *pngDec = nullptr;
static int pngDraw(PNGDRAW *d) {
  int w = d->iWidth > 1024 ? 1024 : d->iWidth;
  pngDec->getLineAsRGB565(d, pngLine, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
  for (int x = 0; x < w; x++) put(x, d->y, pngLine[x]);
  return 1;
}

// ---- JPG: one block of pixels
static int jpgDraw(JPEGDRAW *d) {
  for (int y = 0; y < d->iHeight; y++)
    for (int x = 0; x < d->iWidthUsed; x++) put(d->x + x, d->y + y, d->pPixels[y * d->iWidth + x]);
  return 1;
}

static uint8_t *loadFile(const String &path, int &len) {
  File f = SD.open(path);
  if (!f) return nullptr;
  len = f.size();
  if (len <= 0 || len > 4 * 1024 * 1024) {  // 4 MB limit
    f.close();
    return nullptr;
  }
  uint8_t *buf = (uint8_t *)ps_malloc(len);
  if (buf && f.read(buf, len) != (size_t)len) {
    free(buf);
    buf = nullptr;
  }
  f.close();
  return buf;
}

static void center(int w, int h) {
  offX = (W - w) / 2;  // negative when wider than the panel: center crop
  offY = (H - h) / 2;
}

int playNextImage(uint32_t dwellMs, int (*wait)(uint32_t)) {
  if (files.empty() && !imagesScan()) {
    drawMessage("NO IMAGES", "SD /images", 0x707070);
    return wait(3000);
  }
  if (!canvas) canvas = (uint16_t *)ps_malloc(W * H * 2);
  if (!canvas) return wait(1000);
  String path = files[order++ % files.size()];
  if (order % files.size() == 0) imagesScan();  // reshuffle (and pick up new files) after each full pass
  int len = 0;
  uint8_t *data = loadFile(path, len);
  if (!data) {
    Serial.printf("[images] can't read %s\n", path.c_str());
    return 0;
  }
  memset(canvas, 0, W * H * 2);
  String low = path;
  low.toLowerCase();
  int r = 0;
  uint32_t t0 = millis();
  if (low.endsWith(".gif")) {
    AnimatedGIF *gif = new (ps_malloc(sizeof(AnimatedGIF))) AnimatedGIF();
    gif->begin(GIF_PALETTE_RGB565_LE);
    if (gif->open(data, len, gifDraw)) {
      center(gif->getCanvasWidth(), gif->getCanvasHeight());
      bool first = true;
      while (!r && (first || millis() - t0 < dwellMs)) {
        int delayMs = 0;
        int more = gif->playFrame(false, &delayMs);
        displayBlit565(canvas);
        r = wait(delayMs > 10 ? delayMs : 100);
        if (!more) {  // end of the animation: loop, unless it was a single still frame
          first = false;
          gif->reset();
        }
      }
      gif->close();
    }
    gif->~AnimatedGIF();
    free(gif);
  } else if (low.endsWith(".png")) {
    pngDec = new (ps_malloc(sizeof(PNG))) PNG();
    if (pngDec->openRAM(data, len, pngDraw) == PNG_SUCCESS) {
      center(pngDec->getWidth(), pngDec->getHeight());
      pngDec->decode(nullptr, 0);
      pngDec->close();
      displayBlit565(canvas);
      r = wait(dwellMs);
    }
    pngDec->~PNG();
    free(pngDec);
    pngDec = nullptr;
  } else {
    JPEGDEC *jpg = new (ps_malloc(sizeof(JPEGDEC))) JPEGDEC();
    if (jpg->openRAM(data, len, jpgDraw)) {
      jpg->setPixelType(RGB565_LITTLE_ENDIAN);
      int w = jpg->getWidth(), h = jpg->getHeight(), opt = 0, div = 1;
      // shrink by 1/2, 1/4 or 1/8 until it fits (or as small as it goes)
      const int scales[3][2] = {{JPEG_SCALE_HALF, 2}, {JPEG_SCALE_QUARTER, 4}, {JPEG_SCALE_EIGHTH, 8}};
      for (auto &sc : scales)
        if (w / div > W || h / div > H) {
          opt = sc[0];
          div = sc[1];
        }
      center(w / div, h / div);
      jpg->decode(0, 0, opt);
      jpg->close();
      displayBlit565(canvas);
      r = wait(dwellMs);
    }
    jpg->~JPEGDEC();
    free(jpg);
  }
  free(data);
  return r;
}

// The Yule Log footage is a bright video; on LEDs it looks washed out. Deepen it: a gamma curve pulls the
// mid-tones down (embers stay dark red, only the flame cores stay bright) and a little extra saturation.
static uint8_t fireCurve[256];
static uint16_t fireTone(uint16_t c) {
  int r = ((c >> 11) & 0x1F) * 255 / 31, g = ((c >> 5) & 0x3F) * 255 / 63, b = (c & 0x1F) * 255 / 31;
  int l = (r * 3 + g * 6 + b) / 10;  // saturate: push each channel away from the grey level
  r = std::min(255, std::max(0, l + (r - l) * 13 / 10));
  g = std::min(255, std::max(0, l + (g - l) * 13 / 10));
  b = std::min(255, std::max(0, l + (b - l) * 13 / 10));
  r = fireCurve[r], g = fireCurve[g], b = fireCurve[b];
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
static void fireDraw(GIFDRAW *d) {
  int y = d->iY + d->y;
  for (int i = 0; i < d->iWidth; i++) {
    uint8_t idx = d->pPixels[i];
    if (d->ucHasTransparency && idx == d->ucTransparent) continue;
    put(d->iX + i, y, fireTone(d->pPalette[idx]));
  }
}

// ---- fireplace: Tidbyt's Yule Log loop (64x32, 150 frames at 100 ms), played from flash
int playYuleLog(uint32_t dwellMs, int (*wait)(uint32_t)) {
  if (!canvas) canvas = (uint16_t *)ps_malloc(W * H * 2);
  if (!canvas) return wait(1000);
  AnimatedGIF *gif = new (ps_malloc(sizeof(AnimatedGIF))) AnimatedGIF();
  gif->begin(GIF_PALETTE_RGB565_LE);
  int r = 0;
  if (!fireCurve[255])
    for (int i = 0; i < 256; i++) fireCurve[i] = (uint8_t)(255.0f * powf(i / 255.0f, 1.9f) + 0.5f);
  if (gif->open((uint8_t *)YULE_GIF, sizeof(YULE_GIF), fireDraw)) {
    offX = offY = 0;
    memset(canvas, 0, W * H * 2);
    uint32_t t0 = millis();
    while (!r && millis() - t0 < dwellMs) {
      int delayMs = 0;
      if (!gif->playFrame(false, &delayMs)) gif->reset();
      displayBlit565(canvas);
      r = wait(delayMs > 10 ? delayMs : 100);
    }
    gif->close();
  }
  gif->~AnimatedGIF();
  free(gif);
  return r;
}
