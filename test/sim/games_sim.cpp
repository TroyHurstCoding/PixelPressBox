// Plays every game on its autopilot: how long it lasts, plus a frame of each into out/ (exit 1 if a game dies in under 5 s)
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "Arduino.h"
#include "../../src/render.h"
#include "../../src/games.h"
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
Settings CFG;
extern MatrixPanel_I2S_DMA *simPanel();
static void dumpLand(const char *name) {
  MatrixPanel_I2S_DMA *p = simPanel();
  std::string path = std::string("out/") + name + ".ppm";
  FILE *f = fopen(path.c_str(), "wb");
  fprintf(f, "P6 64 32 255\n");
  for (int y = 0; y < 32; y++) for (int x = 0; x < 64; x++) { uint32_t c = p->shown[y][x]; unsigned char b[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c}; fwrite(b, 1, 3, f); }
  fclose(f);
}
static void dumpTallLocal(const char *name) {  // portrait frame drawn with displaySetRotation(1): the upright 32x64 view
  MatrixPanel_I2S_DMA *p = simPanel();
  FILE *f = fopen((std::string("out/") + name + ".ppm").c_str(), "wb");
  fprintf(f, "P6 32 64 255\n");
  for (int y = 0; y < 64; y++) for (int x = 0; x < 32; x++) { uint32_t c = p->shown[x][63 - y]; unsigned char b[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c}; fwrite(b, 1, 3, f); }
  fclose(f);
}
// Blockfall: portrait frames mid-game, in a line-clear flash and at game over, plus the ready/over screens
static int blockfallFrames() {
  int bad = 0;
  displaySetRotation(1);
  gameReset(G_BLOCKFALL, 7919);
  int f = 0, prev = 0, shotMid = 0, shotFlash = 0;
  for (; f < 30 * 600; f++) {
    GameInput in = gameAutopilot(G_BLOCKFALL);
    if (!gameStep(G_BLOCKFALL, in)) break;
    if (f == 30 * 20) { gameDraw(G_BLOCKFALL, f); dumpTallLocal("G_blockfall_1_mid"); shotMid = 1; }
    if (f == 30 * 60) { gameDraw(G_BLOCKFALL, f); dumpTallLocal("G_blockfall_2_late"); }
    if (!shotFlash && f > 30 * 8 && gameScore(G_BLOCKFALL) - prev >= 100) { f += 2; for (int k = 0; k < 2; k++) gameStep(G_BLOCKFALL, GameInput()); gameDraw(G_BLOCKFALL, f); dumpTallLocal("G_blockfall_3_flash"); shotFlash = 1; }
    prev = gameScore(G_BLOCKFALL);
  }
  printf("BLOCKFALL autopilot (portrait): lasted %.1f s, score %d%s\n", f / 30.0, gameScore(G_BLOCKFALL), f >= 30 * 600 ? " (survived 10 min)" : "");
  // no steering: the stack must top out and report game over
  gameReset(G_BLOCKFALL, 5);
  int g = 0;
  for (; g < 30 * 300; g++) { GameInput in; in.pitch = (g % 40 == 0) ? 0.9f : 0; if (!gameStep(G_BLOCKFALL, in)) break; }
  printf("BLOCKFALL no-steer run: topped out after %.1f s, score %d\n", g / 30.0, gameScore(G_BLOCKFALL));
  if (g >= 30 * 300) bad++;
  gameDraw(G_BLOCKFALL, g); dumpTallLocal("G_blockfall_4_over_board");
  drawGameReady(G_BLOCKFALL, 3); dumpTallLocal("G_blockfall_5_ready");
  drawGameOver(G_BLOCKFALL, 1234, 5000, false, 2); dumpTallLocal("G_blockfall_6_over");
  drawGameOver(G_BLOCKFALL, 123456, 123456, true, 2); dumpTallLocal("G_blockfall_7_over_best");
  displaySetRotation(0);
  if (!shotMid || !shotFlash) bad++;
  return bad + (f < 150);
}
int main() {
  int bad = 0;
  displayBegin();
  for (int g = 0; g < G_COUNT; g++) {
    if (gameIsPortrait(g)) { bad += blockfallFrames(); continue; }
    for (int seed = 1; seed <= 3; seed++) {
      gameReset(g, seed * 7919);
      int f = 0;
      for (; f < 30 * 180; f++) { GameInput in = gameAutopilot(g); if (!gameStep(g, in)) break; if (seed == 1 && f == (g == 0 ? 120 : 240)) { gameDraw(g, f); char n[40]; snprintf(n, 40, "7%d_game_%s", g, gameName(g)); dumpLand(n); } }
      if (f < 150) bad++;
      printf("%-10s seed %d: lasted %5.1f s, score %d\n", gameName(g), seed, f / 30.0, gameScore(g));
    }
  }
  drawGameMenu(0, 42, 3); dumpLand("80_game_menu");
  drawGameOver(0, 137, 137, true, 2); dumpLand("81_game_over");
  return bad ? 1 : 0;
}
