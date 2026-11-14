// motion.h - ADXL345 accelerometer (GY-291 board on the I2C expansion port, address 0x53).
// Gravity in SCREEN terms, the clock's orientation (landscape / portrait / upside down), lying flat, and taps.
// Everything degrades gracefully: with no accelerometer present the clock is simply always landscape.
#pragma once
#include <Arduino.h>

struct Motion {
  bool present = false;
  float gx = 0, gy = 1, gz = 0;  // where "down" points, in g: gx = toward the screen's right edge, gy = toward its
                                 // bottom edge, gz = out of the screen toward you (all in landscape terms)
  int orient = 0;                // 0 landscape, 1 portrait (left edge down), 2 upside down, 3 portrait (right edge down)
                                 // = the panel rotation that makes things read upright
  bool flat = false;             // lying on its back or face (gz dominant)
};

bool motionBegin();              // probe + configure; false if no accelerometer answers
void motionPoll();               // read at most every 20 ms; call often
const Motion &motion();
int motionTakeTap();             // 0 none, 1 single tap, 2 double tap (cleared when read)
bool motionPortrait();           // orient 1 or 3 (or the config forces portrait)
int motionLandscapeRotation();   // 0, or 2 when the clock stands upside down

// ---- pure helpers (unit-tested)
// raw sensor counts (full resolution, 256 per g) -> screen gravity. The board hangs under a fin with its chip side
// facing down: sensor Z = screen up/down, X = screen left/right, Y = depth. zSign/xSign fix the mounting.
void mapAxes(int16_t rx, int16_t ry, int16_t rz, int xSign, int zSign, float &gx, float &gy, float &gz);
// orientation with hysteresis: only changes when one axis clearly dominates; flat keeps the previous one
int classifyOrient(float gx, float gy, float gz, int prev, bool &flat);
