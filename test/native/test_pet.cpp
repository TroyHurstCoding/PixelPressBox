// Desktop test for the pixel pet rules: need drift, caps, feeding, night refill, moods, stages, save format.
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "Arduino.h"
#include <cmath>
#include <cstdio>
#include "../../src/pet.h"
#include "../../src/desk.h"

Settings CFG;

static int fails = 0;
#define CHECK(c)                                          \
  do {                                                    \
    if (!(c)) {                                           \
      printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); \
      fails++;                                            \
    }                                                     \
  } while (0)
static bool near(float a, float b, float e = 0.05f) { return fabsf(a - b) < e; }

static const uint32_t H = 3600, D = 86400, T0 = 1800000000;

// a fresh pet that has already hatched and been fed
static PetState fresh(uint32_t now) {
  PetState s;
  s.born = now - 3 * D;
  s.lastUpdate = s.lastFed = s.lastPlayed = s.lastSeen = now;
  s.hunger = 0; s.joy = 80; s.energy = 100; s.feeds = 5; s.stage = 2;
  return s;
}

int main() {
  // first run: born is set, it is an egg
  {
    PetState s; petTick(s, T0, false);
    CHECK(s.born == T0 && s.lastUpdate == T0 && s.stage == 0);
    CHECK(petMood(s, T0, false) == PET_EGG);
    CHECK(petMood(s, T0, true) == PET_EGG);        // eggs do not sleep
    petTick(s, T0 + 5 * 60, false);
    CHECK(s.stage == 0 && petMood(s, T0 + 5 * 60, false) == PET_EGG);
    petTick(s, T0 + 11 * 60, false);
    CHECK(s.stage == 1 && petMood(s, T0 + 11 * 60, false) != PET_EGG);
  }
  // drift over 1 h and 10 h awake
  {
    PetState s = fresh(T0); petTick(s, T0 + H, false);
    CHECK(near(s.hunger, 4) && near(s.joy, 78) && near(s.energy, 95));
    PetState t = fresh(T0); petTick(t, T0 + 10 * H, false);
    CHECK(near(t.hunger, 40, 0.5f) && near(t.joy, 60, 0.5f) && near(t.energy, 50, 0.5f));
    // ticking in small steps gives the same result as one big tick
    PetState u = fresh(T0);
    for (int i = 1; i <= 600; i++) petTick(u, T0 + i * 60, false);
    CHECK(near(u.hunger, t.hunger, 0.3f) && near(u.energy, t.energy, 0.3f));
  }
  // joy falls faster when very hungry
  {
    PetState s = fresh(T0); s.hunger = 80; s.joy = 80; petTick(s, T0 + H, false);
    CHECK(s.joy < 78 - 2);
  }
  // long gaps are capped at 48 h and everything stays clamped; never dies
  {
    PetState s = fresh(T0); petTick(s, T0 + 30 * D, false);
    CHECK(near(s.hunger, 4 * 48, 0.1f) || s.hunger == 100);
    CHECK(s.hunger <= 100 && s.joy >= 0 && s.energy >= 0 && s.hunger >= 0 && s.joy <= 100 && s.energy <= 100);
    CHECK(s.energy == 0);
    PetState a = fresh(T0), b = fresh(T0);
    petTick(a, T0 + 5 * D, false); petTick(b, T0 + 500 * D, false);
    CHECK(near(a.hunger, b.hunger) && near(a.joy, b.joy) && near(a.energy, b.energy));
    CHECK(a.hunger == 100 && a.joy < 100);
    // a clock that jumps backwards changes nothing
    PetState c = fresh(T0); petTick(c, T0 - 1000, false);
    CHECK(c.hunger == 0 && c.lastUpdate == T0 - 1000);
  }
  // feeding and overfeeding
  {
    PetState s = fresh(T0); s.hunger = 60; s.joy = 50;
    petFeed(s, T0 + 5);
    CHECK(near(s.hunger, 25) && near(s.joy, 55) && s.feeds == 6 && s.lastFed == T0 + 5);
    petFeed(s, T0 + 6);
    CHECK(s.hunger == 0);
    CHECK(petMood(s, T0 + 6, false) != PET_GRUMPY);
    petFeed(s, T0 + 7);                              // already full: grumpy
    CHECK(s.hunger == 0 && s.joy <= 10);
    CHECK(petMood(s, T0 + 7, false) == PET_GRUMPY);
  }
  // play and cheer
  {
    PetState s = fresh(T0); s.joy = 50; s.energy = 50;
    petPlay(s, T0 + 1);
    CHECK(near(s.joy, 53) && near(s.energy, 49) && s.plays == 1 && s.lastPlayed == T0 + 1);
    petCheer(s, T0 + 2);
    CHECK(near(s.joy, 68) && s.cheers == 1);
    s.joy = 95; petCheer(s, T0 + 3); CHECK(s.joy == 100);
    s.energy = 0.5f; petPlay(s, T0 + 4); CHECK(s.energy == 0);
  }
  // night refills energy ~20/h, day drains it
  {
    PetState s = fresh(T0); s.energy = 10;
    petTick(s, T0 + 2 * H, true);
    CHECK(near(s.energy, 50, 0.5f));
    petTick(s, T0 + 10 * H, true);
    CHECK(s.energy == 100);
    petTick(s, T0 + 11 * H, false);
    CHECK(near(s.energy, 95, 0.5f));
  }
  // moods
  {
    uint32_t now = T0;
    PetState s = fresh(now);
    CHECK(petMood(s, now, false) == PET_HAPPY);
    CHECK(petMood(s, now, true) == PET_ASLEEP);
    s.joy = 50; CHECK(petMood(s, now, false) == PET_OK);
    s.joy = 90; s.hunger = 65; CHECK(petMood(s, now, false) == PET_HUNGRY);
    s.hunger = 90; CHECK(petMood(s, now, false) == PET_GRUMPY);      // grumpy beats hungry
    s.hunger = 10; s.energy = 10; CHECK(petMood(s, now, false) == PET_SLEEPY);
    s.energy = 100; s.joy = 10; CHECK(petMood(s, now, false) == PET_GRUMPY);
    // neglect: not fed/played/seen for more than 2 days
    PetState n = fresh(now); n.hunger = 0; n.joy = 80;
    CHECK(petMood(n, now + 2 * D - 10, false) == PET_HAPPY);
    CHECK(petMood(n, now + 2 * D + 10, false) == PET_GRUMPY);
    n.lastSeen = now + 2 * D;
    CHECK(petMood(n, now + 2 * D + 10, false) == PET_HAPPY);
    CHECK(strcmp(petMoodName(PET_HUNGRY), "HUNGRY") == 0 && strcmp(petMoodName(PET_EGG), "EGG") == 0);
    // hungry -> fed -> happy transition through the real rules
    PetState h = fresh(now); h.hunger = 70; h.joy = 90; h.lastSeen = now;
    CHECK(petMood(h, now, false) == PET_HUNGRY);
    petFeed(h, now); CHECK(petMood(h, now, false) == PET_HAPPY);
    // after ~9 h untouched it is hungry again, but not yet grumpy
    PetState g = fresh(now); petTick(g, now + 18 * H, false);
    CHECK(g.hunger > 60 && g.hunger <= 85);
    CHECK(petMood(g, now + 18 * H, false) == PET_HUNGRY || petMood(g, now + 18 * H, false) == PET_SLEEPY);
  }
  // stage progression
  {
    PetState s; petTick(s, T0, false);
    CHECK(s.stage == 0);
    petTick(s, T0 + 15 * 60, false); CHECK(s.stage == 1);
    petTick(s, T0 + 1 * D, false); CHECK(s.stage == 1);
    petTick(s, T0 + 2 * D + 5, false); CHECK(s.stage == 1);        // no meal yet: still a baby
    petFeed(s, T0 + 2 * D + 6);
    petTick(s, T0 + 2 * D + 10, false); CHECK(s.stage == 2);
    petTick(s, T0 + 8 * D, false); CHECK(s.stage == 2);            // needs more care to grow up
    for (int i = 0; i < 10; i++) petFeed(s, T0 + 8 * D + i);
    petTick(s, T0 + 8 * D + 20, false); CHECK(s.stage == 3);
    CHECK(petAgeDays(s, T0 + 8 * D + 20) == 8 && petAgeDays(s, T0) == 0);
    petTick(s, T0 + 8 * D + 30, false); CHECK(s.stage == 3);       // never shrinks
  }
  // pack / unpack
  {
    PetState s = fresh(T0);
    s.hunger = 33.5f; s.joy = 12.25f; s.energy = 99; s.feeds = 300; s.plays = 65000; s.cheers = 7; s.stage = 3;
    s.lastFed = 11; s.lastPlayed = 22; s.lastSeen = 0xFFFFFFFFu;
    uint8_t buf[64];
    size_t n = petPack(s, buf, sizeof buf);
    CHECK(n > 20 && n <= sizeof buf);
    CHECK(petPack(s, buf, n - 1) == 0);
    PetState r;
    CHECK(petUnpack(r, buf, n));
    CHECK(r.born == s.born && r.lastUpdate == s.lastUpdate && r.lastFed == 11 && r.lastPlayed == 22 && r.lastSeen == 0xFFFFFFFFu);
    CHECK(r.hunger == 33.5f && r.joy == 12.25f && r.energy == 99 && r.feeds == 300 && r.plays == 65000 && r.cheers == 7 && r.stage == 3);
    PetState keep = fresh(T0); PetState k2 = keep;
    CHECK(!petUnpack(k2, buf, n - 1)); CHECK(!petUnpack(k2, buf, n + 1)); CHECK(!petUnpack(k2, buf, 0));
    CHECK(k2.born == keep.born && k2.joy == keep.joy);              // rejected: state untouched
    uint8_t bad[64]; memcpy(bad, buf, n);
    bad[0] = 2; CHECK(!petUnpack(k2, bad, n));                      // version mismatch
    memcpy(bad, buf, n); bad[1] = (uint8_t)(n + 1); CHECK(!petUnpack(k2, bad, n));   // size byte mismatch
    memcpy(bad, buf, n); bad[n - 1] = 9; CHECK(!petUnpack(k2, bad, n));              // junk stage
    CHECK(buf[2] == (uint8_t)(s.born & 255) && buf[3] == (uint8_t)((s.born >> 8) & 255));   // little endian
  }
  if (fails) { printf("%d pet test(s) FAILED\n", fails); return 1; }
  printf("all pet tests passed\n");
  return 0;
}
