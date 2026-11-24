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
  bool calibrated = false;       // the mounting was measured from the phone page (System > Motion sensor)
};

bool motionBegin();              // probe + configure; false if no accelerometer answers
void motionPoll();               // read at most every 20 ms; call often
const Motion &motion();
int motionTakeTap();             // 0 none, 1 single tap, 2 double tap (cleared when read)
bool motionTakeShake();          // a few hard shakes in a row (cleared when read): opens the games menu
bool motionPortrait();           // orient 1 or 3 (or the config forces portrait)
int motionLandscapeRotation();   // 0, or 2 when the clock stands upside down

// ---- pure helpers (unit-tested)
// raw sensor counts (full resolution, 256 per g) -> screen gravity. Two mountings ("motion.mount" in config.json):
//   "panel" (onPanel true, the default setting): the board lies flat on the back of the LED panel (on one of its
//           screws), pin row along the lower edge, pins pointing at the back cover:
//           sensor Y = screen up/down, X = left/right, Z = depth.
//   "fin"   (onPanel false): the earlier spot, hanging chip-down under a fin on the back cover:
//           sensor Z = screen up/down, X = screen left/right, Y = depth.
// zSign is the sign of whichever axis runs up/down (found at power-up); xSign is "motion.mirror".
void mapAxes(int16_t rx, int16_t ry, int16_t rz, int xSign, int zSign, float &gx, float &gy, float &gz,
             bool onPanel = false);
// Which sensor axis feeds each screen axis, and with what sign: screen[i] = sign[i] * raw[axis[i]] / 256
// (screen 0 = toward the right edge, 1 = toward the bottom edge, 2 = out of the screen toward you).
struct AxisMap {
  int8_t axis[3] = {0, 1, 2};
  int8_t sign[3] = {-1, -1, 1};
};
AxisMap axisMapFor(bool onPanel, int xSign, int zSign);   // the two known mountings
void applyAxisMap(const AxisMap &m, int16_t rx, int16_t ry, int16_t rz, float &gx, float &gy, float &gz);
// Calibration: raw readings with the clock (1) standing upright in landscape and (2) lying on its back, screen up.
// Works for any mounting; false if either pose wasn't square or both show the same axis.
bool axisMapFromPoses(const int16_t upright[3], const int16_t onBack[3], AxisMap &out);
// orientation: stays put until the clock is turned more than halfway (45 degrees + a little) toward the next one;
// lying nearly flat keeps the previous one
int classifyOrient(float gx, float gy, float gz, int prev, bool &flat);

// phone page: step 1 = standing upright, step 2 = on its back, 0 = forget the calibration. Returns what to show.
const char *motionCalibrate(int step);
void motionRaw(float out[3]);    // smoothed raw sensor reading in g (x, y, z as the chip reports them)
