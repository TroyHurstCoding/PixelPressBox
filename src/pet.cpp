// pet.cpp - "Blip", an original round critter that lives in a tiny room on the clock.
// Rules (needs, mood, stage, save format) are plain functions; drawPet is the 30 fps animation.
#include "pet.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "gfxutil.h"

static const uint32_t HOUR = 3600, DAY = 86400;
static const uint32_t MAX_GAP = 48 * HOUR;       // never count more than this of downtime
static const uint32_t HATCH_SECS = 10 * 60;      // egg hatches 10 minutes after the first run
static float clamp100(float v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

// ---------------------------------------------------------------- rules
static uint32_t lastCare(const PetState &s) { return std::max(s.lastFed, std::max(s.lastPlayed, s.lastSeen)); }

static uint8_t stageFor(const PetState &s, uint32_t now) {
  if (s.born == 0 || now < s.born || now - s.born < HATCH_SECS) return 0;
  int d = (int)((now - s.born) / DAY);
  uint8_t st = 1;
  if (d >= 2 && s.feeds >= 1) st = 2;          // kid needs 2 days and at least one meal
  if (d >= 7 && s.feeds >= 10) st = 3;         // grown needs a week and a bit of care
  return st;
}

void petTick(PetState &s, uint32_t now, bool night) {
  if (s.born == 0) {                           // first run: the egg appears now
    s.born = now;
    s.lastUpdate = s.lastFed = s.lastPlayed = s.lastSeen = now;
  }
  if (s.lastUpdate == 0 || s.lastUpdate > now) s.lastUpdate = now;   // clock jumped back: just resync
  uint32_t gap = std::min(now - s.lastUpdate, MAX_GAP);
  bool asleep = night && stageFor(s, now) > 0;
  while (gap > 0) {                            // 10 minute steps so "hungry > 70" speeds joy loss at the right time
    uint32_t step = std::min<uint32_t>(gap, 600);
    float h = step / (float)HOUR;
    s.joy = clamp100(s.joy - 2.0f * h * (s.hunger > 70 ? 2.5f : 1.0f));
    s.hunger = clamp100(s.hunger + 4.0f * h);
    s.energy = clamp100(s.energy + (asleep ? 20.0f : -5.0f) * h);
    gap -= step;
  }
  s.lastUpdate = now;
  s.hunger = clamp100(s.hunger); s.joy = clamp100(s.joy); s.energy = clamp100(s.energy);
  uint8_t st = stageFor(s, now);
  if (st > s.stage) s.stage = st;              // never shrinks back
}

void petFeed(PetState &s, uint32_t now) {
  bool stuffed = s.hunger < 5;
  s.hunger = clamp100(s.hunger - 35);
  if (stuffed) s.joy = std::min(s.joy, 10.0f); // too much food: grumpy
  else s.joy = clamp100(s.joy + 5);
  s.lastFed = now;
  if (s.feeds < 65535) s.feeds++;
}
void petPlay(PetState &s, uint32_t now) {
  s.joy = clamp100(s.joy + 3);
  s.energy = clamp100(s.energy - 1);
  s.lastPlayed = now;
  if (s.plays < 65535) s.plays++;
}
void petCheer(PetState &s, uint32_t now) {
  s.joy = clamp100(s.joy + 15);
  s.lastPlayed = now;
  if (s.cheers < 65535) s.cheers++;
}

PetMood petMood(const PetState &s, uint32_t now, bool night) {
  if (s.born == 0 || stageFor(s, now) == 0) return PET_EGG;
  if (night) return PET_ASLEEP;
  uint32_t lc = lastCare(s);
  bool neglected = now > lc && now - lc > 2 * DAY;
  if (s.hunger > 85 || neglected || s.joy < 20) return PET_GRUMPY;
  if (s.hunger > 60) return PET_HUNGRY;
  if (s.energy < 20) return PET_SLEEPY;
  if (s.joy >= 70 && s.hunger < 40) return PET_HAPPY;
  return PET_OK;
}
const char *petMoodName(PetMood m) {
  switch (m) {
    case PET_HAPPY: return "HAPPY";
    case PET_OK: return "OK";
    case PET_HUNGRY: return "HUNGRY";
    case PET_GRUMPY: return "GRUMPY";
    case PET_SLEEPY: return "SLEEPY";
    case PET_ASLEEP: return "ASLEEP";
    default: return "EGG";
  }
}
int petAgeDays(const PetState &s, uint32_t now) {
  if (s.born == 0 || now < s.born) return 0;
  return (int)((now - s.born) / DAY);
}

// ---------------------------------------------------------------- save format (little endian)
// [0] version  [1] total size  [2..21] five u32  [22..33] three f32  [34..39] three u16  [40] stage
static const uint8_t PET_VER = 1;
static const size_t PET_SIZE = 41;
static void put32(uint8_t *&p, uint32_t v) { for (int i = 0; i < 4; i++) *p++ = (uint8_t)(v >> (8 * i)); }
static void putf(uint8_t *&p, float f) { uint32_t v; memcpy(&v, &f, 4); put32(p, v); }
static void put16(uint8_t *&p, uint16_t v) { *p++ = (uint8_t)v; *p++ = (uint8_t)(v >> 8); }
static uint32_t get32(const uint8_t *&p) { uint32_t v = 0; for (int i = 0; i < 4; i++) v |= (uint32_t)*p++ << (8 * i); return v; }
static float getf(const uint8_t *&p) { uint32_t v = get32(p); float f; memcpy(&f, &v, 4); return f; }
static uint16_t get16(const uint8_t *&p) { uint16_t v = p[0] | (p[1] << 8); p += 2; return v; }

size_t petPack(const PetState &s, uint8_t *buf, size_t cap) {
  if (!buf || cap < PET_SIZE) return 0;
  uint8_t *p = buf;
  *p++ = PET_VER; *p++ = (uint8_t)PET_SIZE;
  put32(p, s.born); put32(p, s.lastUpdate); put32(p, s.lastFed); put32(p, s.lastPlayed); put32(p, s.lastSeen);
  putf(p, s.hunger); putf(p, s.joy); putf(p, s.energy);
  put16(p, s.feeds); put16(p, s.plays); put16(p, s.cheers);
  *p++ = s.stage;
  return (size_t)(p - buf);
}
bool petUnpack(PetState &s, const uint8_t *buf, size_t len) {
  if (!buf || len != PET_SIZE || buf[0] != PET_VER || buf[1] != PET_SIZE) return false;
  const uint8_t *p = buf + 2;
  PetState t;
  t.born = get32(p); t.lastUpdate = get32(p); t.lastFed = get32(p); t.lastPlayed = get32(p); t.lastSeen = get32(p);
  t.hunger = getf(p); t.joy = getf(p); t.energy = getf(p);
  t.feeds = get16(p); t.plays = get16(p); t.cheers = get16(p);
  t.stage = *p++;
  if (t.stage > 3 || !std::isfinite(t.hunger) || !std::isfinite(t.joy) || !std::isfinite(t.energy)) return false;
  t.hunger = clamp100(t.hunger); t.joy = clamp100(t.joy); t.energy = clamp100(t.energy);
  s = t;
  return true;
}

// ---------------------------------------------------------------- drawing
namespace {
using namespace gu;
const int FLOOR = 26;                     // y of the floor line; feet rest on FLOOR-1

// animation state (statics, advanced one step per drawPet call)
bool inited = false;
float petX = 30, petDir = 1, petTarget = 30, petHop = 0;
int waitT = 0;
float bx = 46, bvx = 0, bh = 0, bvh = 0, ballMoved = 0;
int chaseT = 0, eatF = 0, cheerF = 0, wakeF = 0;
float snackX = 30;
uint32_t tick = 0, seed = 1, kickT = 0;

uint32_t nt(uint32_t c, bool night) {      // night dimming that keeps lit pixels visible
  if (!night) return c;
  auto ch = [](uint32_t v) { return (uint32_t)std::max(0x24, (int)(v * 0.5f)); };
  if ((c & 0xFFFFFF) == 0) return 0;
  return (ch((c >> 16) & 255) << 16) | (ch((c >> 8) & 255) << 8) | ch(c & 255);
}

void ell(float cx, float cy, float rx, float ry, uint32_t base, bool night, bool shade = true) {
  for (int y = (int)floorf(cy - ry); y <= (int)ceilf(cy + ry); y++)
    for (int x = (int)floorf(cx - rx); x <= (int)ceilf(cx + rx); x++) {
      float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
      if (dx * dx + dy * dy > 1.0f) continue;
      uint32_t c = base;
      if (shade) {
        if (dy < -0.45f) c = mix(base, 0xFFFFFF, 0.22f);
        else if (dy > 0.55f) c = mix(base, 0x000000, 0.28f);
      }
      px(x, y, nt(c, night));
    }
}

uint32_t palc(char k) {
  switch (k) {
    case 'r': return 0xF03040; case 'p': return 0xFF5080; case 'y': return 0xFFD030; case 'g': return 0x9098B0;
    case 'G': return 0x40C050; case 'w': return 0xFFFFFF; case 'b': return 0x50A0FF; case 'd': return 0x586070;
    case 'o': return 0xE08020; case 'n': return 0x905020; default: return 0;
  }
}
void sprite(int x, int y, const char *const *rows, int n, bool night) {
  for (int j = 0; j < n; j++)
    for (int i = 0; rows[j][i]; i++)
      if (rows[j][i] != '.') px(x + i, y + j, nt(palc(rows[j][i]), night));
}
const char *HEART[] = {".r.r.", "rrrrr", ".rrr.", "..r.."};
const char *FOOD[] = {"..G..", ".ooo.", "ooooo", ".ooo."};
const char *STORM[] = {".ddd.", "ddddd", "..y..", ".y..."};
const char *ZZ[] = {"bbb", "..b", ".b.", "bbb"};

struct Look { uint32_t body, belly; int r; };
Look lookFor(int stage) {
  switch (stage) {
    case 1: return {0xFFA8C8, 0xFFE0EC, 5};     // baby: pink
    case 2: return {0x70D8FF, 0xCFF3FF, 6};     // kid: sky blue
    default: return {0xB594FF, 0xE6DAFF, 7};    // grown: violet
  }
}

void drawBars(const PetState &s, bool night, int frame) {
  struct B { float v; uint32_t c; int x; } bars[3] = {{100 - s.hunger, 0xFF9020, 1}, {s.joy, 0xFF4890, 22}, {s.energy, 0x40C0FF, 43}};
  for (auto &b : bars) {
    fill(b.x, 0, 20, 3, nt(0x242830, night));
    int w = (int)(b.v * 20 / 100.0f + 0.5f);
    uint32_t c = b.c;
    if (b.v < 25 && (frame / 8) % 2 == 0) c = 0xFF2020;      // low: blink red
    fill(b.x, 0, w, 3, nt(c, night));
    px(b.x, 0, nt(mix(c, 0xFFFFFF, 0.4f), night));
  }
}

void drawRoom(bool night, int frame) {
  fill(0, 4, 64, FLOOR - 4, night ? 0x000000 : 0x16213C);
  fill(0, FLOOR, 64, 6, nt(0x5A3E24, night));
  fill(0, FLOOR, 64, 1, nt(0x8A6234, night));
  for (int x = 6; x < 64; x += 16) px(x, FLOOR + 3, nt(0x7A5530, night));
  // window
  uint32_t fr = nt(0xA08058, night);
  rect(47, 7, 11, 10, fr);
  fill(48, 8, 9, 8, night ? 0x0A1236 : 0x4098E8);
  px(52, 8, fr); fill(52, 8, 1, 8, fr); fill(48, 12, 9, 1, fr);
  if (night) {
    disc(54.5f, 10.5f, 1.6f, 0xE8E8B0);
    disc(55.5f, 10.0f, 1.2f, 0x0A1236);
    px(50, 9, (frame / 20) % 2 ? 0x909090 : 0xFFFFFF);
    px(50, 14, 0x707070);
  } else {
    disc(54.5f, 10.5f, 1.6f, 0xFFD840);
    px(49, 14, 0xFFFFFF); px(50, 14, 0xFFFFFF); px(50, 13, 0xFFFFFF);
  }
}

// the pet sprite. (cx, footY) = bottom centre. mood drives the face
void drawBlip(int stage, float cx, float footY, float dir, PetMood mood, int frame, bool night, bool eating, bool cheering,
              float chew, float squash) {
  Look L = lookFor(stage);
  float r = (float)L.r;
  float rx = r * (1 + 0.07f * squash), ry = r * (1 - 0.07f * squash);
  float cy = footY - ry;
  uint32_t body = L.body, belly = L.belly;
  if (mood == PET_GRUMPY) body = mix(body, 0xC85050, 0.25f);
  bool blink = (frame % 110) < 4;
  int look = dir > 0 ? 1 : -1;
  // tail and ears behind/around the body
  if (stage >= 3) {
    int tx = (int)floorf(cx - look * (rx + 1)), ty = (int)floorf(cy + ry * 0.4f);
    px(tx, ty, nt(body, night)); px(tx - look, ty - 1, nt(body, night)); px(tx - look, ty - 2 + (frame / 10) % 2, nt(0xFFD030, night));
  }
  if (stage >= 2) {
    int ey = (int)floorf(cy - ry);
    uint32_t ec = nt(mix(body, 0x000000, 0.1f), night), in = nt(mix(body, 0xFF80A0, 0.6f), night);
    int ex = (int)floorf(cx - rx * 0.55f), ex2 = (int)floorf(cx + rx * 0.55f - 1);
    px(ex, ey, ec); px(ex, ey - 1, ec); px(ex + 1, ey, in); px(ex2, ey, ec); px(ex2, ey - 1, ec); px(ex2 - 1, ey, in);
    if (stage == 2) { px(ex, ey - 2, ec); px(ex2, ey - 2, ec); }
  }
  if (stage == 1) { px((int)floorf(cx), (int)floorf(cy - ry) - 1, nt(body, night)); px((int)floorf(cx) + 1, (int)floorf(cy - ry) - 2, nt(body, night)); }
  // feet
  int fy = (int)floorf(footY);
  uint32_t foot = nt(mix(body, 0x000000, 0.45f), night);
  int fo = (int)floorf(rx * 0.45f);
  fill((int)floorf(cx) - fo - 1, fy, 2, 1, foot);
  fill((int)floorf(cx) + fo, fy, 2, 1, foot);
  // body + belly
  ell(cx, cy, rx, ry, body, night);
  ell(cx, cy + ry * 0.5f, rx * 0.5f, ry * 0.42f, belly, night, false);
  if (stage >= 3) {                          // antenna with a glowing bulb
    int ax = (int)floorf(cx) + (look > 0 ? 0 : 1), ay = (int)floorf(cy - ry);
    px(ax, ay - 1, nt(0x8060D0, night)); px(ax, ay - 2, nt(0x8060D0, night));
    px(ax, ay - 3, nt((frame / 12) % 2 ? 0xFFE060 : 0xFFA030, night));
  }
  // face
  int ex = (int)floorf(cx - rx * 0.5f) + (look > 0 ? 1 : 0);
  int ex2 = (int)floorf(cx + rx * 0.5f) + (look > 0 ? 1 : 0) - 1;
  int eyY = (int)floorf(cy - ry * 0.2f);
  uint32_t dark = nt(0x1A1424, night), cheek = nt(0xFF7090, night);
  bool happyEyes = cheering || mood == PET_HAPPY && (frame / 40) % 4 == 0;
  if (mood == PET_ASLEEP) {
    fill(ex - 1, eyY + 1, 3, 1, dark); fill(ex2 - 1, eyY + 1, 3, 1, dark);
  } else if (happyEyes) {                    // ^ ^
    px(ex - 1, eyY + 1, dark); px(ex, eyY, dark); px(ex + 1, eyY + 1, dark);
    px(ex2 - 1, eyY + 1, dark); px(ex2, eyY, dark); px(ex2 + 1, eyY + 1, dark);
  } else if (blink || mood == PET_SLEEPY) {
    fill(ex - 1, eyY + 1, 2, 1, dark); fill(ex2, eyY + 1, 2, 1, dark);
    if (mood == PET_SLEEPY && !blink) { px(ex - 1, eyY, dark); px(ex2 + 1, eyY, dark); }
  } else {
    int eh = stage == 1 ? 3 : 2;             // babies have big eyes
    fill(ex, eyY, 1, eh, dark); fill(ex2, eyY, 1, eh, dark);
    px(ex, eyY, nt(0xFFFFFF, night)); px(ex2, eyY, nt(0xFFFFFF, night));
    if (mood == PET_GRUMPY) {                // angry brows
      px(ex - 1, eyY - 1, dark); px(ex, eyY - 1, dark); px(ex2, eyY - 1, dark); px(ex2 + 1, eyY - 1, dark);
      px(ex - 1, eyY - 2, dark); px(ex2 + 1, eyY - 2, dark);
      px(ex - 1, eyY - 1, nt(0xFF3030, night)); px(ex2 + 1, eyY - 1, nt(0xFF3030, night));
    }
  }
  int mx = (int)floorf(cx) , my = eyY + 3 + (stage >= 3);
  if (mood != PET_ASLEEP && mood != PET_SLEEPY) { px(ex - 1, my - 1, cheek); px(ex2 + 1, my - 1, cheek); }
  if (eating) {
    int o = chew > 0.5f ? 1 : 0;             // chomp
    fill(mx - 1, my, 2, 1 + o, dark);
  } else if (cheering || mood == PET_HAPPY) {
    fill(mx - 1, my, 2, 1, dark); px(mx - 2, my - 1, dark); px(mx + 1, my - 1, dark);
  } else if (mood == PET_HUNGRY) {
    fill(mx - 1, my, 2, 2, dark);            // open, wanting
  } else if (mood == PET_GRUMPY) {
    fill(mx - 1, my, 2, 1, dark); px(mx - 2, my + 1, dark); px(mx + 1, my + 1, dark);
  } else if (mood == PET_SLEEPY) {
    px(mx, my, dark); px(mx - 1, my, dark); px(mx, my + 1, dark); px(mx - 1, my + 1, dark);   // yawn
  } else if (mood == PET_ASLEEP) {
    px(mx, my, dark);
  } else {
    fill(mx - 1, my, 2, 1, dark);
  }
}

void drawEgg(const PetState &s, uint32_t now, bool night, int frame, bool tap) {
  float age = (s.born && now > s.born) ? (float)(now - s.born) : 0;
  bool wob = (frame % 120) < 24 || tap;
  float tilt = wob ? sinf(frame * 0.7f) * 1.0f : 0;
  float cx = 32 + tilt, cy = FLOOR - 7;
  ell(cx, cy, 5, 7, 0xF2EAD0, night);
  for (int i = 0; i < 6; i++) {              // spots
    static const int sp[6][2] = {{-2, -3}, {2, -2}, {-1, 1}, {2, 3}, {-3, 3}, {0, -5}};
    px((int)floorf(cx) + sp[i][0], (int)floorf(cy) + sp[i][1], nt(0x58C0A0, night));
    px((int)floorf(cx) + sp[i][0] + 1, (int)floorf(cy) + sp[i][1], nt(0x58C0A0, night));
  }
  if (age > HATCH_SECS * 0.6f) {             // cracks appear before hatching
    int x = (int)floorf(cx), y = (int)floorf(cy) - 1;
    uint32_t k = nt(0x403020, night);
    px(x - 2, y, k); px(x - 1, y + 1, k); px(x, y, k); px(x + 1, y + 1, k); px(x + 2, y, k);
  }
  fill(32 - 3, FLOOR - 1, 7, 1, nt(0x3A2814, night));    // shadow
  if (night) { text3("Z", 40, 12 - (frame / 20) % 2, nt(0x6080D0, true)); }
}

float ballFloorY() { return FLOOR - 3; }
}  // namespace

float petBallSpeed() { return ballMoved; }

void drawPet(const PetState &s, uint32_t now, bool night, int frame, bool tap, bool cheer, float gx, float gy, bool accel) {
  tick++;
  if (!inited) { inited = true; seed = (uint32_t)now ^ 0x5bd1e995u; }
  PetMood mood = petMood(s, now, night);
  int stage = s.stage ? s.stage : (mood == PET_EGG ? 0 : 1);
  if (mood != PET_EGG && stage == 0) stage = 1;
  bool asleep = mood == PET_ASLEEP;
  Look L = lookFor(stage ? stage : 1);

  // ---- events
  if (tap && !asleep && mood != PET_EGG && eatF == 0) { eatF = 36; snackX = petX + 5 * petDir; }
  if (cheer && mood != PET_EGG) { cheerF = 66; seed = gu::hash32(seed + tick); }
  if (eatF > 0) eatF--;
  if (cheerF > 0) cheerF--;
  bool eating = eatF > 0, cheering = cheerF > 0 && !asleep;
  if (!cheering && cheerF > 0) cheerF = 0;

  // ---- ball physics: rolls with gravity when the accelerometer is live, wanders otherwise
  float lo = 4, hi = 59;
  float pbx = bx;
  if (accel) bvx += gx * 0.55f;
  else {
    kickT++;
    if (kickT > 150 + (gu::hash32(seed + tick / 150) % 120)) { kickT = 0; bvx += ((gu::hash32(seed ^ tick) & 1) ? 1.f : -1.f) * (0.8f + (gu::hash32(tick) % 10) * 0.08f); }
  }
  bvx *= accel ? 0.975f : 0.96f;
  bvx = std::max(-3.0f, std::min(3.0f, bvx));
  bx += bvx;
  if (bx < lo) { bx = lo; bvx = -bvx * 0.6f; if (fabsf(bvx) > 0.4f) bvh = 0.9f; }
  if (bx > hi) { bx = hi; bvx = -bvx * 0.6f; if (fabsf(bvx) > 0.4f) bvh = 0.9f; }
  if (fabsf(bvx) < 0.02f) bvx = 0;
  bh += bvh; bvh -= 0.18f;                    // little bounce height
  if (bh < 0) { bh = 0; bvh = 0; }
  ballMoved = fabsf(bx - pbx);
  if (asleep || mood == PET_EGG) ballMoved = 0;
  if (ballMoved > 0.25f) chaseT = 75;
  else if (chaseT > 0) chaseT--;

  // ---- pet movement
  bool still = asleep || mood == PET_EGG || eating;
  float speed = 0;
  if (!still) {
    if (chaseT > 0 && !cheering) {
      float want = bx + (bx > petX ? -(L.r + 2) : (L.r + 2));
      want = std::max(10.0f, std::min(54.0f, want));
      petTarget = want; waitT = 0;
      speed = 0.7f;
    } else if (waitT > 0) {
      waitT--;
    } else if (fabsf(petTarget - petX) < 0.6f) {
      waitT = 60 + gu::hash32(seed + tick) % 120;
      petTarget = 10 + (float)(gu::hash32(seed * 3 + tick) % 44);
    } else speed = (mood == PET_SLEEPY ? 0.12f : mood == PET_GRUMPY ? 0.15f : 0.22f);
    if (cheering) speed = 0;
    float d = petTarget - petX;
    if (fabsf(d) > 0.6f && speed > 0) { petX += d > 0 ? speed : -speed; petDir = d > 0 ? 1 : -1; }
    else speed = 0;
  }

  // ---- draw
  gfxBegin();
  drawRoom(night, frame);
  drawBars(s, night, frame);
  if (mood == PET_EGG) {
    drawEgg(s, now, night, frame, tap);
    gfxEnd();
    return;
  }

  float bobS = sinf(frame * 2 * (float)M_PI / 60.0f);
  if (asleep) {
    // bed on the left, pet curled up in it, Zzz
    int bxl = 12;
    fill(bxl, FLOOR - 4, 24, 4, nt(0x6A4A30, true));
    fill(bxl, FLOOR - 5, 24, 1, nt(0x5878C0, true));
    fill(bxl, FLOOR - 4, 2, 5, nt(0x805A38, true)); fill(bxl + 22, FLOOR - 4, 2, 5, nt(0x805A38, true));
    fill(bxl + 2, FLOOR - 7, 6, 2, nt(0xD0D4E8, true));              // pillow
    float sq = 0.5f + 0.5f * bobS;                                    // slow breathing
    float rx = L.r + 1 + sq * 0.5f, ry = L.r - 2.2f - sq * 0.4f;
    float cx = bxl + 12, cy = FLOOR - 6 - ry + 1;
    uint32_t body = nt(L.body, true);
    (void)body;
    // draw curled body via the face routine's pieces
    ell(cx, cy, rx, ry, L.body, true);
    ell(cx - 1, cy + ry * 0.2f, rx * 0.5f, ry * 0.5f, L.belly, true, false);
    int eyY = (int)floorf(cy - 1);
    uint32_t dark = nt(0x1A1424, true);
    fill((int)cx - 4, eyY, 2, 1, dark); fill((int)cx - 1, eyY, 2, 1, dark);
    px((int)cx - 3, eyY + 2, dark); px((int)cx - 2, eyY + 2, dark);
    if (stage >= 2) { px((int)cx - 4, (int)(cy - ry) - 1, nt(L.body, true)); px((int)cx + 1, (int)(cy - ry) - 1, nt(L.body, true)); }
    fill(bxl + 4, FLOOR - 6 - (int)ry + 1, 20, 3, 0);                // (clear nothing, keeps sprite on top of bed)
    ell(cx + 3, cy + ry * 0.55f, rx * 0.75f, ry * 0.5f, 0x4868B8, true, false);    // blanket
    for (int i = 0; i < 3; i++) {
      int ph = (frame / 6 + i * 6) % 24;
      int zx = (int)cx + 6 + i * 4 + ph / 8, zy = (int)(cy - ry) - 3 - i * 3 - ph / 6;
      if (zy >= 4) text3("Z", zx, zy, nt(0x7090E8, true));
    }
  } else {
    float footY = FLOOR - 1;
    float hop = 0, squash = bobS * 0.5f;
    if (speed > 0) hop = fabsf(sinf(frame * 0.5f)) * 1.4f;
    if (cheering) { hop = fabsf(sinf((66 - cheerF) * 0.33f)) * 6.0f; squash = hop > 0.5f ? -0.8f : 1.0f; }
    int drawY = (int)floorf(footY - hop + 0.5f);
    // shadow
    fill((int)petX - L.r + 1, FLOOR, 2 * L.r - 2, 1, nt(0x3A2814, night));
    drawBlip(stage, petX, (float)drawY + (hop > 0 ? 0 : 0), petDir, mood, frame, night, eating, cheering,
             (eatF % 6) < 3 ? 1.0f : 0.0f, squash);
    // mood icon
    int ix = (int)petX + L.r - 1, iy = (int)(FLOOR - L.r * 2) - 7 - (int)(hop) + ((frame / 15) % 2);
    iy = std::max(iy, 5);
    if (!eating && !cheering) {
      if (mood == PET_HAPPY) sprite(ix, iy, HEART, 4, night);
      else if (mood == PET_HUNGRY) sprite(ix, iy, FOOD, 4, night);
      else if (mood == PET_GRUMPY) sprite(ix, iy, STORM, 4, night);
      else if (mood == PET_SLEEPY) { sprite(ix, iy, ZZ, 4, night); }
    }
    // snack falling and being eaten
    if (eating) {
      int t = 36 - eatF;
      float mouthY = FLOOR - 1 - L.r * 1.0f + 1;
      if (t < 12) {
        float sy = 5 + (mouthY - 5) * (t / 12.0f);
        float sx = snackX - (snackX - petX) * (t / 12.0f);
        disc(sx, sy, 1.5f, nt(0xE8A030, night)); px((int)sx, (int)sy - 1, nt(0xFFE080, night));
      } else if (t < 32) {
        float k = 1.0f - (t - 12) / 20.0f;
        if (k > 0.2f) disc(petX + petDir * 1.0f, mouthY, 0.5f + 1.2f * k, nt(0xE8A030, night));
        if (t % 6 == 3) { px((int)petX + 3, FLOOR - 2, nt(0xE8A030, night)); px((int)petX - 3, FLOOR - 2, nt(0xE8A030, night)); }
      } else if (t < 36) {
        sprite((int)petX - 2, (int)(FLOOR - L.r * 2) - 6, HEART, 4, night);
      }
    }
  }

  // ball (hidden while asleep)
  if (!asleep) {
    float by = ballFloorY() - bh + 1;
    fill((int)bx - 1, FLOOR, 3, 1, nt(0x3A2814, night));
    disc(bx, by, 2.0f, nt(0xF04848, night));
    px((int)bx - 1, (int)by - 1, nt(0xFFFFFF, night));
    int sx = (int)(bx + sinf(bx * 0.8f) * 1.2f);
    px(sx, (int)by + 1, nt(0xFFD0D0, night));
  }

  // confetti
  if (cheering) {
    float t = (float)(66 - cheerF);
    for (int i = 0; i < 44; i++) {
      uint32_t h = gu::hash32(seed * 131 + i);
      float x0, y0, vx, vy;
      if (i % 3 != 0) {                       // burst from the pet
        x0 = petX; y0 = FLOOR - 10;
        vx = ((int)(h % 100) - 50) / 50.0f * 0.9f; vy = -(0.8f + ((h >> 8) % 100) / 100.0f * 1.0f);
      } else {                                // rain from the top
        x0 = (float)((h >> 4) % 64); y0 = 4 - (float)((h >> 12) % 14);
        vx = (((h >> 20) % 100) - 50) / 160.0f; vy = 0.25f + ((h >> 16) % 100) / 400.0f;
      }
      float x = x0 + vx * t + sinf(t * 0.3f + i) * 0.6f, y = y0 + vy * t + 0.5f * 0.05f * t * t;
      if (y >= FLOOR + 5) y = FLOOR + 5;
      uint32_t c = gu::hsv(((h >> 24) & 255) / 256.0f, 1.0f, 1.0f);
      px((int)x, (int)y, nt(c, night));
      if (i % 3 == 0) px((int)x + 1, (int)y, nt(c, night));
    }
  }
  gfxEnd();
}
