// pet.h - a little creature that lives on the clock. Tap = feed, tilt = play (it chases a ball that rolls with
// gravity), it sleeps at night, cheers when a favorite team scores and gets grumpy if ignored for days.
// The state is a small struct the firmware saves in Preferences (petPack/petUnpack); the rules are pure
// functions so they can be unit-tested on a computer.
#pragma once
#include <Arduino.h>

struct PetState {
  uint32_t born = 0;          // UTC epoch seconds
  uint32_t lastUpdate = 0;    // last time petTick ran
  uint32_t lastFed = 0, lastPlayed = 0, lastSeen = 0;
  float hunger = 0;           // 0 full .. 100 starving
  float joy = 80;             // 0 miserable .. 100 delighted
  float energy = 100;         // 0 exhausted .. 100 rested
  uint16_t feeds = 0, plays = 0, cheers = 0;
  uint8_t stage = 0;          // 0 egg, 1 baby, 2 kid, 3 grown (by age and care)
};

enum PetMood { PET_HAPPY, PET_OK, PET_HUNGRY, PET_GRUMPY, PET_SLEEPY, PET_ASLEEP, PET_EGG };

// advance the needs to `now` (hunger rises ~4/h, joy falls ~2/h, energy falls while awake and refills while
// asleep); a long gap (clock off) is capped so it never comes back dead. `night` = the clock is in night mode.
void petTick(PetState &s, uint32_t now, bool night);
void petFeed(PetState &s, uint32_t now);      // hunger -35, joy +5; overfeeding (hunger < 5) makes it grumpy
void petPlay(PetState &s, uint32_t now);      // joy +3 per call (called while the ball is moving), energy -1
void petCheer(PetState &s, uint32_t now);     // a favorite team scored: joy +15
PetMood petMood(const PetState &s, uint32_t now, bool night);
const char *petMoodName(PetMood m);           // "HAPPY", "HUNGRY"...
int petAgeDays(const PetState &s, uint32_t now);

// fixed-size binary form for Preferences (putBytes / getBytes); petUnpack returns false on a size/version mismatch
size_t petPack(const PetState &s, uint8_t *buf, size_t cap);
bool petUnpack(PetState &s, const uint8_t *buf, size_t len);

// draws one frame (landscape 64x32): the pet in a little room, a tiny needs bar row, and reactions.
// tap = fed this frame (show a snack falling); cheer = a team just scored (jumping, confetti);
// gx, gy = gravity in screen terms for the ball (accel false = the ball wanders on its own).
void drawPet(const PetState &s, uint32_t now, bool night, int frame, bool tap, bool cheer, float gx, float gy,
             bool accel);
// how far the ball moved this frame (pixels), so the caller can call petPlay while it's being played with
float petBallSpeed();
