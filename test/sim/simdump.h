// simdump.h - shared by the batch 5 screen sims (sim5_*.cpp): write the panel to out/<name>.ppm
#pragma once
#include <cstdio>
#include <string>
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
extern MatrixPanel_I2S_DMA *simPanel();
// landscape 64x32 frame
inline void dump(const char *name) {
  MatrixPanel_I2S_DMA *p = simPanel();
  FILE *f = fopen((std::string("out/") + name + ".ppm").c_str(), "wb");
  fprintf(f, "P6 64 32 255\n");
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 64; x++) {
      uint32_t c = p->shown[y][x];
      unsigned char b[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c};
      fwrite(b, 1, 3, f);
    }
  fclose(f);
}
// portrait screens drawn with displaySetRotation(1): writes the upright 32x64 view
inline void dumpTall(const char *name) {
  MatrixPanel_I2S_DMA *p = simPanel();
  FILE *f = fopen((std::string("out/") + name + ".ppm").c_str(), "wb");
  fprintf(f, "P6 32 64 255\n");
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 32; x++) {
      uint32_t c = p->shown[x][63 - y];
      unsigned char b[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c};
      fwrite(b, 1, 3, f);
    }
  fclose(f);
}
