// feeds3.h - batch 4 data: win probability and top performers for a game (ESPN summary), the golf leaderboard,
// each favorite team's next game (ESPN team schedule) and planes overhead (OpenSky Network, anonymous).
#pragma once
#include <ArduinoJson.h>
#include "model.h"

// one summary download for a game: its win-probability line and, if wanted, each team's top performers
bool fetchGameExtras(const Game &g, bool wantLeaders, WinProb &wp, std::vector<Highlight> &hl);
bool fetchGolf(const String &tour, GolfBoard &out);
bool fetchNextGame(const String &league, const String &team, time_t now, NextGame &out);
bool fetchFlights(float lat, float lon, int radiusKm, std::vector<Flight> &out);

// ---- pure helpers (unit-tested)
void parseWinProb(const JsonDocument &summary, const Game &g, WinProb &out);
void parseHighlights(const JsonDocument &summary, const Game &g, std::vector<Highlight> &out);
void parseGolf(const JsonDocument &d, GolfBoard &out);
bool parseNextGame(const JsonDocument &schedule, const String &league, const String &team, time_t now, NextGame &out);
void parseFlights(const JsonDocument &d, float lat, float lon, int radiusKm, std::vector<Flight> &out);
float distanceKm(float lat1, float lon1, float lat2, float lon2);
float bearingDeg(float lat1, float lon1, float lat2, float lon2);
