// Plays every game on its autopilot: how long it lasts, plus a frame of each into out/ (exit 1 if a game dies in under 5 s)
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "Arduino.h"
#include "../../src/render.h"
#include "../../src/games.h"
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
Settings CFG;
extern MatrixPanel_I2S_DMA *simPanel();
static void dump(const char *name) {
  MatrixPanel_I2S_DMA *p = simPanel();
  std::string path = std::string("out/") + name + ".ppm";
  FILE *f = fopen(path.c_str(), "wb");
  fprintf(f, "P6 64 32 255\n");
  for (int y = 0; y < 32; y++) for (int x = 0; x < 64; x++) { uint32_t c = p->shown[y][x]; unsigned char b[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c}; fwrite(b, 1, 3, f); }
  fclose(f);
}
int main() {
  int bad = 0;
  displayBegin();
  for (int g = 0; g < G_COUNT; g++) {
    for (int seed = 1; seed <= 3; seed++) {
      gameReset(g, seed * 7919);
      int f = 0;
      for (; f < 30 * 180; f++) { GameInput in = gameAutopilot(g); if (!gameStep(g, in)) break; if (seed == 1 && f == (g == 0 ? 120 : 240)) { gameDraw(g, f); char n[40]; snprintf(n, 40, "7%d_game_%s", g, gameName(g)); dump(n); } }
      if (f < 150) bad++;
      printf("%-10s seed %d: lasted %5.1f s, score %d\n", gameName(g), seed, f / 30.0, gameScore(g));
    }
  }
  drawGameMenu(0, 42, 3); dump("80_game_menu");
  drawGameOver(0, 137, 137, true, 2); dump("81_game_over");
  return bad ? 1 : 0;
}
