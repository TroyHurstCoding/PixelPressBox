// Renders the pixel pet in several situations: test/sim/out/P*.ppm
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "Arduino.h"
#include <cstdlib>
#include "../../src/desk.h"
#include "../../src/render.h"
#include "../../src/pet.h"
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#include "simdump.h"

Settings CFG;

static PetState mk(uint32_t now, double ageDays, int stage, float hunger, float joy, float energy) {
  PetState s;
  s.born = now - (uint32_t)(ageDays * 86400);
  s.lastUpdate = s.lastFed = s.lastPlayed = s.lastSeen = now;
  s.hunger = hunger; s.joy = joy; s.energy = energy; s.stage = stage; s.feeds = 20;
  return s;
}
int main() {
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();
  displayBegin();
  uint32_t now = (uint32_t)parseIsoUtc("2026-11-12T20:00:00");
  int f = 0;
  auto run = [&](const PetState &s, bool night, int n, bool tap, bool cheer, float gx, bool accel) {
    for (int i = 0; i < n; i++) drawPet(s, now, night, f++, tap && i == 0, cheer && i == 0, gx, 0, accel);
  };
  PetState egg = mk(now, 0, 0, 10, 80, 100); egg.born = now - 480;
  run(egg, false, 5, false, false, 0, false); dump("P1_egg");
  PetState baby = mk(now, 0.5, 1, 10, 90, 90);
  run(baby, false, 130, false, false, 0, false); dump("P2_baby_happy");
  PetState kid = mk(now, 4, 2, 78, 45, 70);
  run(kid, false, 90, false, false, 0, false); dump("P3_kid_hungry");
  PetState grown = mk(now, 10, 3, 92, 20, 60);
  run(grown, false, 90, false, false, 0, false); dump("P4_grown_grumpy");
  PetState gh = mk(now, 10, 3, 10, 90, 90);
  run(gh, false, 70, false, false, 0, false); dump("P4b_grown_happy");
  for (int st = 1; st <= 3; st++) {
    PetState a = mk(now, st == 1 ? 1 : st == 2 ? 3 : 9, st, 30, 60, 80);
    run(a, true, 40, false, false, 0, false); dump(("P5_asleep_" + std::to_string(st)).c_str());
  }
  PetState k2 = mk(now, 4, 2, 50, 70, 70);
  run(k2, false, 40, false, false, 0, false);
  drawPet(k2, now, false, f++, true, false, 0, 0, false);
  run(k2, false, 8, false, false, 0, false); dump("P6_eat_fall");
  run(k2, false, 10, false, false, 0, false); dump("P6b_eat_chew");
  PetState g2 = mk(now, 10, 3, 20, 85, 80);
  run(g2, false, 20, false, false, 0, false);
  drawPet(g2, now, false, f++, false, true, 0, 0, false);
  run(g2, false, 20, false, false, 0, false); dump("P7_cheer");
  run(g2, false, 15, false, false, 0, false); dump("P7b_cheer2");
  PetState k3 = mk(now, 4, 2, 20, 80, 80);
  run(k3, false, 10, false, false, 0, true);
  run(k3, false, 25, false, false, 1.0f, true); dump("P8_chase_a");
  run(k3, false, 30, false, false, -1.0f, true); dump("P8_chase_b");
  PetState kd = mk(now, 4, 2, 50, 60, 10);
  run(kd, false, 60, false, false, 0, false); dump("P9_kid_sleepy");
  return 0;
}
