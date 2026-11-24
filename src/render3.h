// render3.h - batch 4 screens: binary, sun-arc, city, world and sand clocks; win probability, golf, next game,
// top performers and fantasy points; the sound visualizer, Game of Life and planes overhead.
#pragma once
#include "model.h"

// ---- clocks
void drawBinaryClock(time_t now);
void drawSunArcClock(time_t now, float lat, float lon);
void drawCityClock(time_t now, int frame);
void drawWorldClock(time_t now);                                   // CFG.worldClocks
void drawSandClock(time_t now, float gx, float gy, bool restart);  // gravity in screen terms (g), down = +gy

// ---- sports
void drawWinProb(const WinProb &wp, time_t now, bool showStatus);
int golfPages(const GolfBoard &b);
void drawGolf(const GolfBoard &b, int page);
void drawNextGame(const NextGame &n, time_t now, bool showLine);
void drawHighlight(const Highlight &h, const String &result);       // result: "W 26-9"
void drawFantasyTotals(const FptsBoard &b, int frame);
int fantasyPages(const FptsBoard &b, int side);                     // 4 players per page
void drawFantasyPage(const FptsBoard &b, int side, int page, int frame);

// ---- ambient
void drawVisualizer(const uint8_t *bands, int n, bool mic, int frame);  // bands 0-255, low to high
void drawLife(int frame, bool restart, bool poke);
void drawFlights(const std::vector<Flight> &f, int sel, int radiusKm, int frame);

// ---- helpers (unit-tested)
// local wall-clock time for a city: utc + its offset, plus an hour inside its daylight-saving window
time_t worldLocal(time_t utc, const WorldCity &c, bool *dst = nullptr);
// a compact 3x5 font (digits, capitals, a few symbols): returns the width drawn
int text3(const String &s, int x, int y, uint32_t color);
int text3Width(const String &s);
