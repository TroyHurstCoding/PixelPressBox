// Screen sim for render5b (toys + maze): writes test/sim/out/B*.ppm
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "Arduino.h"
#include <cstdlib>
#include "../../src/render.h"
#include "../../src/render5.h"
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#include "simdump.h"

Settings CFG;

static int litInCol(int x) {
  MatrixPanel_I2S_DMA *p = simPanel();
  int n = 0;
  for (int y = 0; y < 32; y++) if (p->shown[y][x]) n++;
  return n;
}

int main() {
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();
  displayBegin();

  // ---- eyes
  {
    int f = 0, blinkDone = 0;
    for (; f < 40; f++) drawEyes(f, 0, 0, 0);
    dump("B1_eyes_normal");
    for (; f < 800 && !blinkDone; f++) {
      drawEyes(f, 0, 0, 0);
      int n = litInCol(15);
      if (n > 0 && n < 14) { dump("B1_eyes_blink"); blinkDone = 1; }
    }
    for (int k = 0; k < 40; k++, f++) drawEyes(f, -0.9f, 0, 0);
    dump("B1_eyes_left");
    for (int k = 0; k < 40; k++, f++) drawEyes(f, 0.3f, -0.9f, 0);
    dump("B1_eyes_up");
    for (int k = 0; k < 40; k++, f++) drawEyes(f, 0.8f, 0.8f, 0);
    dump("B1_eyes_downright");
    for (int fr = 0; fr < 70; fr++) drawEyes(fr, 0, 0, 1);
    dump("B1_eyes_excited");
    for (int fr = 70; fr < 100; fr++) drawEyes(fr, 0, 0, 1);
    dump("B1_eyes_hearts");
    for (int fr = 0; fr < 50; fr++) drawEyes(fr, 0, 0, 2);
    dump("B1_eyes_sleepy");
    int d2 = 0;
    for (int fr = 50; fr < 600 && !d2; fr++) {
      drawEyes(fr, 0, 0, 2);
      int n = litInCol(15);
      if (n > 0 && n < 6) { dump("B1_eyes_sleepy_blink"); d2 = 1; }
    }
  }

  // ---- 8 ball
  {
    for (int f = 0; f < 20; f++) drawEightBall(f, false);
    dump("B2_8ball_idle");
    drawEightBall(20, true);
    for (int f = 21; f < 28; f++) drawEightBall(f, false);
    dump("B2_8ball_rising");
    for (int f = 28; f < 70; f++) drawEightBall(f, false);
    dump("B2_8ball_answer1");
    drawEightBall(70, true);
    for (int f = 71; f < 120; f++) drawEightBall(f, false);
    dump("B2_8ball_answer2");
    drawEightBall(120, true);
    for (int f = 121; f < 170; f++) drawEightBall(f, false);
    dump("B2_8ball_answer3");
  }

  // ---- party tools
  {
    drawParty(0, 0, false);
    dump("B3_dice_idle");
    drawParty(0, 1, true);
    for (int f = 2; f < 12; f++) drawParty(0, f, false);
    dump("B3_dice_roll");
    for (int f = 12; f < 50; f++) drawParty(0, f, false);
    dump("B3_dice_set");
    drawParty(1, 0, true);
    for (int f = 1; f < 10; f++) drawParty(1, f, false);
    dump("B3_coin_flip");
    for (int f = 10; f < 18; f++) drawParty(1, f, false);
    dump("B3_coin_flip2");
    for (int f = 18; f < 60; f++) drawParty(1, f, false);
    dump("B3_coin_set");
    drawParty(1, 60, true);
    for (int f = 61; f < 110; f++) drawParty(1, f, false);
    dump("B3_coin_set2");
    drawParty(2, 0, false);
    dump("B3_wheel_idle");
    drawParty(2, 1, true);
    for (int f = 2; f < 22; f++) drawParty(2, f, false);
    dump("B3_wheel_spin");
    for (int f = 22; f < 120; f++) drawParty(2, f, false);
    dump("B3_wheel_set");
  }

  // ---- sand timer
  {
    drawSandTimer(0, 0, 135, 20);
    dump("B4_sand_set");
    drawSandTimer(300, 250, 0, 20);
    dump("B4_sand_run_full");
    drawSandTimer(300, 150, 0, 21);
    dump("B4_sand_run_half");
    drawSandTimer(300, 20, 0, 22);
    dump("B4_sand_run_low");
    drawSandTimer(120, 7, 0, 23);
    dump("B4_sand_last");
    drawSandTimer(300, 0, 0, 0);
    dump("B4_sand_done_a");
    drawSandTimer(300, 0, 0, 9);
    dump("B4_sand_done_b");
    drawSandTimer(900, 600, 0, 12);
    dump("B4_sand_10min");
  }

  // ---- maze
  {
    int f = 0;
    for (; f < 50; f++) drawMaze(f);
    dump("B5_maze_carve");
    for (; f < 125; f++) drawMaze(f);
    dump("B5_maze_carve_end");
    for (; f < 150; f++) drawMaze(f);
    dump("B5_maze_flood");
    for (; f < 190; f++) drawMaze(f);
    dump("B5_maze_trace");
    for (; f < 260; f++) drawMaze(f);
    dump("B5_maze_solved");
  }
  puts("sim5_b done");
  return 0;
}
