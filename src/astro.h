// astro.h - where the sun and the bright planets are, computed on the clock (no internet, no API key).
// Method: Paul Schlyter's "How to compute planetary positions" (stjarnhimlen.se), good to about a degree,
// which is plenty for "Jupiter is low in the east".
#pragma once
#include <ctime>
#include <cstdint>
#include <vector>

struct PlanetPos {
  const char *name;   // "JUPITER"
  char letter;        // 'J'
  uint32_t color;
  double alt, az;     // degrees; az 0 = north, 90 = east
};

// all five naked-eye planets (Mercury..Saturn), above the horizon or not
void planetPositions(time_t t, double lat, double lon, std::vector<PlanetPos> &out);
// the point on Earth with the sun straight overhead (degrees)
void sunSubpoint(time_t t, double &lat, double &lon);
double sunAltitude(time_t t, double lat, double lon);
// "SW", "E", ...
const char *compass8(double az);
