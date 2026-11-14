// games.h - tilt games for the accelerometer: Dodger (fly a plane through the trees), TiltyBird, MarbleRun,
// Catch, Breakout, Racer and Balance. Each game is a little state machine driven 30 times a second.
// Without an accelerometer each game has an autopilot, so it can still be shown off (and tested).
#pragma once
#include <Arduino.h>

enum GameId { G_DODGER, G_BIRD, G_MARBLE, G_CATCH, G_BREAKOUT, G_RACER, G_BALANCE, G_COUNT };

struct GameInput {
  float roll = 0;   // -1..1: tilt toward the right edge = positive
  float pitch = 0;  // -1..1: lean the top back (away from you) = positive, relative to how it stood at the start
  bool tap = false;
};

const char *gameName(int id);   // "DODGER"
const char *gameHint(int id);   // "TILT TO BANK"
void gameReset(int id, uint32_t seed);
bool gameStep(int id, const GameInput &in);   // one frame (1/30 s); false = game over
void gameDraw(int id, int frame);
int gameScore(int id);
GameInput gameAutopilot(int id);              // a simple player, for demos without an accelerometer

void drawGameMenu(int sel, int best, int frame);
void drawGameReady(int id, int frame);
void drawGameOver(int id, int score, int best, bool newBest, int frame);
