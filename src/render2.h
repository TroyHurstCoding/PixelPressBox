// render2.h - batch 3 screens: aquarium, double pendulum, Newton's cradle, tilt-able water, the portrait screens
// (beer glass, tall clock, hourglass, lava lamp), Fly the W, the space-station map, planets tonight, surf.
// Gravity arguments are in the screen's own terms (gx toward its right edge, gy toward its bottom), in g.
#pragma once
#include "model.h"

void drawAquarium(int frame);
void drawDoublePendulum(int frame, bool restart);
// accel = an accelerometer is driving gravity (otherwise it runs a demo); kick = a tap just happened
void drawCradle(int frame, float gx, float gy, bool accel, bool kick, bool restart);
void drawWater(int frame, float gx, float gy, bool accel, bool restart);

// portrait (32 wide x 64 tall; the caller sets the panel rotation)
void drawBeer(int frame, float gx, float gy, bool accel, bool tap, bool restart);
void drawPortraitClock(time_t now);
void drawHourglass(int frame, float gy, bool accel, bool restart);
void drawLava(int frame, bool restart);

void drawFlyW(const GameResult &r, uint32_t teamColor, int frame);
void drawIssMap(const IssFix &f, time_t now, float homeLat, float homeLon, int frame);
void drawPlanets(time_t now, float lat, float lon, int page);   // page 0 = sky view, 1 = list
void drawSurf(const SurfReport &s, const String &name, int frame);
