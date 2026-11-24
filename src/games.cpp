#include "games.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

MatrixPanel_I2S_DMA *gfxPanel();
void gfxBegin();
void gfxEnd();
uint16_t gfxColor(uint32_t c);
void gfxText(const String &s, int x, int y, uint32_t color, int size);

static inline void px(int x, int y, uint32_t c) {
  if (x >= 0 && x < 64 && y >= 0 && y < 32) gfxPanel()->drawPixel(x, y, gfxColor(c));
}
static inline void fill(int x, int y, int w, int h, uint32_t c) {
  if (w > 0 && h > 0) gfxPanel()->fillRect(x, y, w, h, gfxColor(c));
}
static inline void txt(const String &s, int x, int y, uint32_t c, int size = 1) { gfxText(s, x, y, c, size); }
static void txtC(const String &s, int y, uint32_t c, int size = 1) { txt(s, (64 - (int)s.length() * 6 * size + size) / 2, y, c, size); }
static void txtR(const String &s, int y, uint32_t c) { txt(s, 64 - (int)s.length() * 6 + 1, y, c); }
static uint32_t mix(uint32_t a, uint32_t b, float t) {
  t = std::max(0.0f, std::min(1.0f, t));
  auto ch = [&](int sh) { return (uint32_t)(((a >> sh) & 255) * (1 - t) + ((b >> sh) & 255) * t) << sh; };
  return ch(16) | ch(8) | ch(0);
}
static uint32_t rs = 1;
static uint32_t rnd() {
  rs ^= rs << 13;
  rs ^= rs >> 17;
  rs ^= rs << 5;
  return rs;
}
static float fr() { return (rnd() & 0xFFFFFF) / 16777216.0f; }
static float clampf(float v, float a, float b) { return std::max(a, std::min(b, v)); }
static void disc(float cx, float cy, float r, uint32_t c) {
  for (int y = (int)floorf(cy - r); y <= (int)ceilf(cy + r); y++)
    for (int x = (int)floorf(cx - r); x <= (int)ceilf(cx + r); x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      if (dx * dx + dy * dy <= r * r) px(x, y, c);
    }
}
static void line(int x0, int y0, int x1, int y1, uint32_t c) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for (int n = 0; n < 200; n++) {
    px(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) err += dy, x0 += sx;
    if (e2 <= dx) err += dx, y0 += sy;
  }
}

static const char *NAMES[G_COUNT] = {"DODGER", "TILTYBIRD", "MARBLERUN", "CATCH", "BREAKOUT", "RACER", "BALANCE",
                                     "PONG", "SIMON", "LANDER", "REACTION"};
static const char *HINTS[G_COUNT] = {"TILT BANK", "LEAN BACK", "TILT ROLL", "TILT MOVE",
                                     "TILT PAD", "TILT STEER", "TILT LEVEL", "TILT PAD", "TILT+TAP",
                                     "LEAN=BURN", "TAP ON GO"};
const char *gameName(int id) { return NAMES[id % G_COUNT]; }
const char *gameHint(int id) { return HINTS[id % G_COUNT]; }

static int score = 0, t = 0;

// =============================================================== Dodger: fly through a forest, seen from behind
struct Tree {
  float wx, z;
  uint8_t kind;
};
static Tree trees[18];
static float dgX = 0, dgSpeed = 0.1f, dgRoll = 0, dgDist = 0;

static void dodgerReset() {
  dgX = 0;
  dgSpeed = 0.10f;
  dgRoll = 0;
  dgDist = 0;
  for (int i = 0; i < 18; i++) {
    trees[i] = {(fr() - 0.5f) * 7, 0.9f + i * 0.65f, (uint8_t)(rnd() % 3)};
    if (trees[i].z < 7 && fabsf(trees[i].wx) < 1.3f) trees[i].wx += trees[i].wx < 0 ? -1.5f : 1.5f;  // a clear start
  }
}
static bool dodgerStep(const GameInput &in) {
  dgRoll += (in.roll - dgRoll) * 0.25f;
  dgX = clampf(dgX + dgRoll * 0.09f, -4.5f, 4.5f);
  dgSpeed = std::min(0.32f, 0.10f + dgDist * 0.00012f);
  dgDist += dgSpeed;
  score = (int)(dgDist * 2);
  for (auto &tr : trees) {
    tr.z -= dgSpeed;
    if (tr.z < 0.15f) {
      tr.z += 11.7f;
      tr.wx = dgX + (fr() - 0.5f) * 7;
      tr.kind = rnd() % 3;
    }
    if (tr.z > 0.35f && tr.z < 0.75f && fabsf(tr.wx - dgX) < 0.42f) return false;  // crunch
  }
  return true;
}
static void dodgerDraw(int frame) {
  gfxBegin();
  auto hy = [&](float x) { return 9.0f + (x - 32) * dgRoll * 0.22f; };  // banked horizon
  Tree *order[18];
  for (int i = 0; i < 18; i++) order[i] = &trees[i];
  std::sort(order, order + 18, [](Tree *a, Tree *b) { return a->z > b->z; });
  for (Tree *tr : order) {
    if (tr->z > 11) continue;
    float sx = 32 + (tr->wx - dgX) * 20 / tr->z;
    float base = hy(sx) + 21 / tr->z;
    float h = std::max(5.0f, 15 / tr->z), w = std::max(4.0f, 9.0f / tr->z);
    int tw = std::max(1, (int)(w / 3));
    uint32_t leaf = tr->kind == 0 ? 0x8DE471 : tr->kind == 1 ? 0x62D6A0 : 0xC6E973;
    fill((int)(sx - tw / 2.0f), (int)(base - h * 0.35f), tw, (int)(h * 0.35f) + 1, 0xBA8E67);
    for (int k = 0; k < (int)(h * 0.75f); k++) {  // a pine: widest at the bottom of the leaves
      float ww = w * (k + 1) / (h * 0.75f);
      fill((int)(sx - ww / 2), (int)(base - h * 0.3f - (h * 0.75f - k)), std::max(1, (int)ww), 1, leaf);
    }
  }
  static const char *PLANE[5] = {".....#.....", ".....#.....", "..#######..", "#####O#####", "....###...."};
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < 11; c++) {
      char ch = PLANE[r][c];
      if (ch == '.') continue;
      int dy = (int)lroundf((c - 5) * dgRoll * 0.45f);
      px(27 + c, 24 + r + dy, ch == 'O' ? 0x404040 : (r == 3 ? 0xE0E0E8 : 0xC8C8D0));
    }
  if ((frame / 3) % 2) px(32, 29, 0xFF8030);  // engine glow
  txtR(String(score), 0, 0xFFFFFF);
  gfxEnd();
}
static GameInput dodgerAuto() {  // push away from the nearest trees ahead, drift back to the middle
  GameInput in;
  float push = -dgX * 0.08f;
  for (auto &tr : trees) {
    if (tr.z > 4 || tr.z < 0.3f) continue;
    float dx = dgX - tr.wx;
    if (fabsf(dx) < 1.4f) push += (dx >= 0 ? 1 : -1) * (1.4f - fabsf(dx)) * (4.2f - tr.z) * 0.6f;
  }
  in.roll = clampf(push, -1, 1);
  return in;
}

// =============================================================== TiltyBird
struct Pipe {
  float x;
  int gap;
  bool passed;
};
static Pipe pipes[3];
static float bdY = 16, bdSpeed = 0.7f;
static void birdReset() {
  bdY = 16;
  bdSpeed = 0.7f;
  for (int i = 0; i < 3; i++) pipes[i] = {70.0f + i * 26, 8 + (int)(rnd() % 14), false};
}
static bool birdStep(const GameInput &in) {
  float target = 15 - in.pitch * 14;
  bdY += (clampf(target, 1, 29) - bdY) * 0.18f;
  bdSpeed = std::min(1.3f, 0.7f + score * 0.03f);
  for (auto &p : pipes) {
    p.x -= bdSpeed;
    if (p.x < -6) {
      float far = 0;
      for (auto &q : pipes) far = std::max(far, q.x);
      p = {far + 26, 7 + (int)(rnd() % 16), false};
    }
    if (!p.passed && p.x + 5 < 13) p.passed = true, score++;
    if (p.x < 17 && p.x + 5 > 12 && (bdY < p.gap - 5.5f || bdY > p.gap + 4.5f)) return false;
  }
  return bdY > 0 && bdY < 30;
}
static void birdDraw(int frame) {
  gfxBegin();
  for (int y = 0; y < 30; y++) fill(0, y, 64, 1, mix(0x4FA8E8, 0xBEE6FF, y / 30.0f));
  fill(0, 30, 64, 2, 0xC8A060);
  for (auto &p : pipes) {
    int x = (int)p.x;
    fill(x, 0, 5, p.gap - 6, 0x3CB043);
    fill(x, p.gap + 6, 5, 30 - (p.gap + 6), 0x3CB043);
    fill(x, 0, 1, p.gap - 6, 0x7CD87F);
    fill(x, p.gap + 6, 1, 30 - (p.gap + 6), 0x7CD87F);
    fill(x - 1, p.gap - 7, 7, 1, 0x2A8A30);
    fill(x - 1, p.gap + 6, 7, 1, 0x2A8A30);
  }
  int by = (int)lroundf(bdY);
  fill(13, by - 1, 4, 3, 0xFFD23F);
  px(16, by - 1, 0x000000);
  px(17, by, 0xFF7A00);
  px(13 + ((frame / 3) % 2), by + ((frame / 3) % 2 ? 1 : -1), 0xF0F0F0);  // flapping wing
  txtC(String(score), 1, 0xFFFFFF);
  gfxEnd();
}
static GameInput birdAuto() {
  GameInput in;
  const Pipe *next = nullptr;
  for (auto &p : pipes)
    if (p.x + 5 >= 12 && (!next || p.x < next->x)) next = &p;
  float gapY = next ? next->gap : 15;
  in.pitch = clampf((15 - gapY) / 14, -1, 1);
  return in;
}

// =============================================================== MarbleRun: tilt a ball through a maze
static uint8_t mzWall[32][64];
static float mbX, mbY, mbVX, mbVY;
static int mzGX, mzGY, mzTime;
static void mazeBuild() {
  const int CW = 16, CH = 8;  // 4 px cells: 3 px of path, 1 px of wall
  memset(mzWall, 1, sizeof mzWall);
  bool seen[CH][CW] = {};
  int stack[CW * CH][2], sp = 0;
  stack[sp][0] = 0, stack[sp][1] = 0, sp++;
  seen[0][0] = true;
  auto open = [&](int cx, int cy) {
    for (int y = 0; y < 3; y++)
      for (int x = 0; x < 3; x++) mzWall[cy * 4 + 1 + y][cx * 4 + 1 + x] = 0;
  };
  open(0, 0);
  while (sp) {
    int cx = stack[sp - 1][0], cy = stack[sp - 1][1];
    int dirs[4] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) std::swap(dirs[i], dirs[rnd() % (i + 1)]);
    bool moved = false;
    for (int d : dirs) {
      int nx = cx + (d == 0) - (d == 1), ny = cy + (d == 2) - (d == 3);
      if (nx < 0 || ny < 0 || nx >= CW || ny >= CH || seen[ny][nx]) continue;
      seen[ny][nx] = true;
      open(nx, ny);
      for (int k = 0; k < 3; k++) {  // knock down the wall between
        if (nx != cx) mzWall[cy * 4 + 1 + k][std::max(cx, nx) * 4] = 0;
        else mzWall[std::max(cy, ny) * 4][cx * 4 + 1 + k] = 0;
      }
      stack[sp][0] = nx, stack[sp][1] = ny, sp++;
      moved = true;
      break;
    }
    if (!moved) sp--;
  }
  mbX = 2.0f, mbY = 2.0f;
  mbVX = mbVY = 0;
  mzGX = 15, mzGY = 7;
}
static void marbleReset() {
  mazeBuild();
  mzTime = 45 * 30;
}
static bool wallAt(float x, float y) {  // the marble is 2x2 at (x, y)
  for (int dy = 0; dy < 2; dy++)
    for (int dx = 0; dx < 2; dx++) {
      int ix = (int)floorf(x) + dx, iy = (int)floorf(y) + dy;
      if (ix < 0 || iy < 0 || ix > 63 || iy > 31 || mzWall[iy][ix]) return true;
    }
  return false;
}
static bool marbleStep(const GameInput &in) {
  mbVX = clampf((mbVX + in.roll * 0.06f) * 0.96f, -0.7f, 0.7f);
  mbVY = clampf((mbVY - in.pitch * 0.06f) * 0.96f, -0.7f, 0.7f);  // lean the top back: it rolls up
  if (!wallAt(mbX + mbVX, mbY)) mbX += mbVX;
  else mbVX *= -0.3f;
  if (!wallAt(mbX, mbY + mbVY)) mbY += mbVY;
  else mbVY *= -0.3f;
  if ((int)(mbX + 1) / 4 == mzGX && (int)(mbY + 1) / 4 == mzGY) {
    score++;
    mazeBuild();
    mzTime += 15 * 30;
  }
  return --mzTime > 0;
}
static void marbleDraw(int frame) {
  gfxBegin();
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 64; x++)
      if (mzWall[y][x]) px(x, y, 0x3A4A7A);
  fill(mzGX * 4 + 1, mzGY * 4 + 1, 3, 3, (frame / 6) % 2 ? 0x101010 : 0x20FF60);  // the hole
  fill((int)mbX, (int)mbY, 2, 2, 0xE8E8F0);
  px((int)mbX, (int)mbY, 0xFFFFFF);
  int bar = std::min(64, mzTime / 30);  // seconds left, along the top edge
  fill(0, 0, bar, 1, mzTime < 10 * 30 ? 0xFF3030 : 0xFFC000);
  gfxEnd();
}
static GameInput marbleAuto() {  // breadth-first search to the goal, then roll toward the next cell
  GameInput in;
  int cx = (int)(mbX + 1) / 4, cy = (int)(mbY + 1) / 4;
  int prev[8][16];
  memset(prev, -1, sizeof prev);
  int q[128], qh = 0, qt = 0;
  q[qt++] = mzGY * 16 + mzGX;
  prev[mzGY][mzGX] = mzGY * 16 + mzGX;
  while (qh < qt) {
    int c = q[qh++], x = c % 16, y = c / 16;
    const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (auto &d : D) {
      int nx = x + d[0], ny = y + d[1];
      if (nx < 0 || ny < 0 || nx > 15 || ny > 7 || prev[ny][nx] >= 0) continue;
      int wx = d[0] ? std::max(x, nx) * 4 : x * 4 + 2, wy = d[1] ? std::max(y, ny) * 4 : y * 4 + 2;
      if (mzWall[wy][wx]) continue;
      prev[ny][nx] = c;
      q[qt++] = ny * 16 + nx;
    }
  }
  int nxt = prev[cy][cx];
  if (nxt < 0) return in;
  float tx = (nxt % 16) * 4 + 1.5f, ty = (nxt / 16) * 4 + 1.5f;
  in.roll = clampf((tx - mbX) * 0.5f - mbVX * 2, -1, 1);
  in.pitch = -clampf((ty - mbY) * 0.5f - mbVY * 2, -1, 1);
  return in;
}

// =============================================================== Catch
struct Drop {
  float x, y, vy;
  uint8_t kind;  // 0 apple, 1 gold, 2 bomb
  bool live;
};
static Drop drops[8];
static float ctX = 32;
static int ctLives = 3, ctNext = 0;
static void catchReset() {
  ctX = 32;
  ctLives = 3;
  ctNext = 20;
  for (auto &d : drops) d.live = false;
}
static bool catchStep(const GameInput &in) {
  ctX += (clampf(32 + in.roll * 30, 5, 59) - ctX) * 0.35f;
  if (--ctNext <= 0) {
    for (auto &d : drops)
      if (!d.live) {
        int r = rnd() % 100;
        d = {3 + fr() * 58, -2, 0.25f + std::min(0.5f, score * 0.01f) + fr() * 0.15f,
             (uint8_t)(r < 8 ? 1 : r < 25 ? 2 : 0), true};
        break;
      }
    ctNext = std::max(10, 40 - score / 2);
  }
  for (auto &d : drops) {
    if (!d.live) continue;
    d.y += d.vy;
    if (d.y >= 27 && d.y < 30 && fabsf(d.x - ctX) < 5.5f) {
      d.live = false;
      if (d.kind == 2) return false;
      score += d.kind == 1 ? 5 : 1;
    } else if (d.y > 32) {
      d.live = false;
      if (d.kind != 2 && --ctLives <= 0) return false;
    }
  }
  return true;
}
static void catchDraw(int frame) {
  gfxBegin();
  for (int y = 0; y < 32; y++) fill(0, y, 64, 1, mix(0x0B1030, 0x203060, y / 32.0f));
  for (auto &d : drops) {
    if (!d.live) continue;
    int x = (int)d.x, y = (int)d.y;
    if (d.kind == 0) {
      fill(x - 1, y, 3, 3, 0xE53935);
      px(x, y - 1, 0x5D4037);
    } else if (d.kind == 1) {
      px(x, y - 1, 0xFFD700), fill(x - 1, y, 3, 1, 0xFFD700), px(x, y + 1, 0xFFD700);
      if ((frame / 4) % 2) px(x, y, 0xFFFFFF);
    } else {
      fill(x - 1, y, 3, 3, 0x505050);
      px(x + 1, y - 1, (frame / 3) % 2 ? 0xFF4000 : 0xFFD000);  // fuse
    }
  }
  int bx = (int)lroundf(ctX);
  fill(bx - 5, 29, 11, 3, 0x8D6E63);
  fill(bx - 5, 28, 1, 1, 0x8D6E63);
  fill(bx + 5, 28, 1, 1, 0x8D6E63);
  for (int i = 0; i < ctLives; i++) fill(1 + i * 4, 1, 3, 2, 0xFF4060);
  txtR(String(score), 0, 0xFFFFFF);
  gfxEnd();
}
static GameInput catchAuto() {
  GameInput in;
  const Drop *best = nullptr;
  for (auto &d : drops)
    if (d.live && d.kind != 2 && (!best || d.y > best->y)) best = &d;
  float target = best ? best->x : 32;
  for (auto &d : drops)
    if (d.live && d.kind == 2 && d.y > 18 && fabsf(d.x - target) < 6) target += d.x > target ? -10 : 10;
  in.roll = clampf((target - 32) / 30, -1, 1);
  return in;
}

// =============================================================== Breakout (tilt the paddle)
static uint8_t bricks[4][8];
static float boX, boY, boVX, boVY, boPad;
static int boLives, boLevel;
static void breakoutServe() {
  boX = boPad;
  boY = 26;
  boVX = (rnd() & 1 ? 1 : -1) * 0.45f;
  boVY = -0.55f - boLevel * 0.08f;
}
static void breakoutBricks() {
  for (auto &r : bricks)
    for (auto &b : r) b = 1;
}
static void breakoutReset() {
  boPad = 32;
  boLives = 3;
  boLevel = 0;
  breakoutBricks();
  breakoutServe();
}
static bool breakoutStep(const GameInput &in) {
  boPad += (clampf(32 + in.roll * 28, 5, 59) - boPad) * 0.4f;
  for (int s = 0; s < 2; s++) {
    boX += boVX / 2;
    boY += boVY / 2;
    if (boX < 0 || boX > 63) boVX = -boVX, boX = clampf(boX, 0, 63);
    if (boY < 0) boVY = -boVY, boY = 0;
    int bc = (int)boX / 8, br = ((int)boY - 3) / 3;
    if (boY >= 3 && br >= 0 && br < 4 && bc >= 0 && bc < 8 && bricks[br][bc]) {
      bricks[br][bc] = 0;
      boVY = -boVY;
      score += 10;
    }
    if (boY >= 29 && boY < 30.5f && boVY > 0 && fabsf(boX - boPad) < 6) {
      boVY = -fabsf(boVY);
      boVX = clampf((boX - boPad) * 0.12f, -0.8f, 0.8f);
      if (fabsf(boVX) < 0.2f) boVX = (boVX < 0 || (boVX == 0 && (rnd() & 1))) ? -0.2f : 0.2f;  // never straight up
    }
  }
  bool left = false;
  for (auto &r : bricks)
    for (auto &b : r) left |= b;
  if (!left) {
    boLevel++;
    breakoutBricks();
    breakoutServe();
  }
  if (boY > 32) {
    if (--boLives <= 0) return false;
    breakoutServe();
  }
  return true;
}
static void breakoutDraw(int frame) {
  static const uint32_t ROWC[4] = {0xFF3B30, 0xFF9500, 0xFFCC00, 0x34C759};
  gfxBegin();
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 8; c++)
      if (bricks[r][c]) fill(c * 8, 3 + r * 3, 7, 2, ROWC[r]);
  fill((int)lroundf(boPad) - 5, 30, 11, 1, 0xE0E0E0);
  px((int)boX, (int)boY, 0xFFFFFF);
  for (int i = 0; i < boLives; i++) px(1 + i * 2, 0, 0xFF4060);
  txtR(String(score), 23, 0x404040);
  (void)frame;
  gfxEnd();
}
static GameInput breakoutAuto() {
  GameInput in;
  float aim = boX + ((t++ / 90) % 2 ? 2.5f : -2.5f);  // hit off-center so the ball wanders
  in.roll = clampf((aim - 32) / 28, -1, 1);
  return in;
}

// =============================================================== Racer (top-down, road scrolls toward you)
struct Car {
  float x, y, vy;
  uint32_t col;
  bool live;
};
static Car cars[4];
static float rcX = 32, rcDist = 0, rcSpeed = 0.5f;
static int rcNext = 0;
static float roadCenter(float d) { return 32 + sinf(d * 0.013f) * 9 + sinf(d * 0.031f) * 4; }
static void racerReset() {
  rcX = 32;
  rcDist = 0;
  rcSpeed = 0.5f;
  rcNext = 40;
  for (auto &c : cars) c.live = false;
}
static bool racerStep(const GameInput &in) {
  rcX = clampf(rcX + in.roll * 1.1f, 2, 62);
  float center = roadCenter(rcDist + 6);
  bool offroad = fabsf(rcX - center) > 13;
  rcSpeed = std::min(1.4f, rcSpeed + 0.0008f) * (offroad ? 0.97f : 1.0f);
  rcSpeed = std::max(rcSpeed, 0.3f);
  rcDist += rcSpeed;
  score = (int)(rcDist / 4);
  if (--rcNext <= 0) {
    for (auto &c : cars)
      if (!c.live) {
        static const uint32_t COLS[4] = {0x2196F3, 0xFFEB3B, 0xE040FB, 0x00E5FF};
        c = {roadCenter(rcDist + 34) + ((int)(rnd() % 3) - 1) * 8.0f, -6, 0, COLS[rnd() % 4], true};
        c.vy = 0.25f;  // slower than you: you catch up
        break;
      }
    rcNext = std::max(25, 70 - score / 8);
  }
  for (auto &c : cars) {
    if (!c.live) continue;
    c.y += rcSpeed - c.vy;
    if (c.y > 34) c.live = false;
    if (c.y > 19 && c.y < 30 && fabsf(c.x - rcX) < 4) return false;
  }
  return true;
}
static void racerDraw(int frame) {
  gfxBegin();
  for (int y = 0; y < 32; y++) {
    float d = rcDist + (32 - y);  // the top of the screen is further down the road
    int c = (int)lroundf(roadCenter(d));
    bool stripe = ((int)(d / 3)) & 1;
    fill(0, y, 64, 1, stripe ? 0x2E7D32 : 0x357A38);
    fill(c - 14, y, 28, 1, 0x424242);
    px(c - 14, y, stripe ? 0xFF2020 : 0xFFFFFF);  // kerbs
    px(c + 13, y, stripe ? 0xFF2020 : 0xFFFFFF);
    if (((int)(d / 4)) % 2) px(c - 5, y, 0xD0D0D0), px(c + 4, y, 0xD0D0D0);  // lane dashes
  }
  auto car = [&](float x, float y, uint32_t col) {
    int ix = (int)lroundf(x) - 2, iy = (int)lroundf(y);
    fill(ix, iy, 5, 7, col);
    fill(ix + 1, iy + 1, 3, 2, 0x202830);  // windshield
    px(ix - 1, iy + 1, 0x101010), px(ix + 5, iy + 1, 0x101010), px(ix - 1, iy + 5, 0x101010), px(ix + 5, iy + 5, 0x101010);
  };
  for (auto &c : cars)
    if (c.live) car(c.x, c.y, c.col);
  car(rcX, 23, 0xFF3D00);
  txtR(String(score), 0, 0xFFFFFF);
  (void)frame;
  gfxEnd();
}
static GameInput racerAuto() {
  GameInput in;
  float target = roadCenter(rcDist + 6);
  for (auto &c : cars)
    if (c.live && c.y > 2 && c.y < 26 && fabsf(c.x - target) < 6) target = c.x > target ? c.x - 9 : c.x + 9;
  in.roll = clampf((target - rcX) * 0.25f, -1, 1);
  return in;
}

// =============================================================== Balance: keep the ball on the plank
static float blAng = 0, blS = 0, blV = 0, blWind = 0;
static void balanceReset() {
  blAng = 0;
  blS = (fr() - 0.5f) * 4;
  blV = 0;
  blWind = 0;
}
static bool balanceStep(const GameInput &in) {
  blAng += (in.roll * 0.45f - blAng) * 0.2f;
  t++;
  float gust = 0.004f + std::min(0.02f, t * 0.00002f);
  blWind = clampf(blWind + (fr() - 0.5f) * gust, -0.03f, 0.03f);
  blV += sinf(blAng) * 0.12f + blWind;
  blV *= 0.995f;
  blS += blV;
  score = t / 30;
  return fabsf(blS) < 25;
}
static void balanceDraw(int frame) {
  gfxBegin();
  for (int y = 0; y < 32; y++) fill(0, y, 64, 1, mix(0x1A1A2E, 0x2E2E50, y / 32.0f));
  for (int k = 0; k < 4; k++) px(31 - k, 27 + k, 0x9E9E9E), px(32 + k, 27 + k, 0x9E9E9E), fill(31 - k, 27 + k, 2 * k + 2, 1, 0x757575);
  fill(26, 31, 12, 1, 0x9E9E9E);
  float ca = cosf(blAng), sa = sinf(blAng);
  line((int)lroundf(32 - 26 * ca), (int)lroundf(26 - 26 * sa), (int)lroundf(32 + 26 * ca), (int)lroundf(26 + 26 * sa), 0xC8A060);
  float bx = 32 + blS * ca, by = 26 + blS * sa - 2.2f;
  disc(bx, by, 2.0f, 0x40C4FF);
  px((int)bx - 1, (int)by - 1, 0xFFFFFF);
  int w = (int)lroundf(blWind * 400);  // wind arrow
  if (abs(w) > 1) line(32, 3, 32 + w, 3, 0x80A0C0);
  txtR(String(score), 0, 0xFFFFFF);
  (void)frame;
  gfxEnd();
}
static GameInput balanceAuto() {
  GameInput in;
  in.roll = clampf(-(blS * 0.05f + blV * 1.6f), -1, 1);
  return in;
}

// =============================================================== Pong: you at the bottom, the clock at the top
static float pgX = 32, cpX = 32, bX = 32, bY = 16, bVX = 0.6f, bVY = 0.7f;
static int pgLives = 3, pgServe = 0;
static float cpAim = 0;  // where on its paddle the clock aims this rally (its mistakes)
static void pongServe(bool towardYou) {
  bX = 32, bY = 16;
  float sp = 0.85f + std::min(0.6f, score * 0.04f);
  float a = (fr() - 0.5f) * 1.1f;
  bVX = sp * sinf(a);
  bVY = sp * cosf(a) * (towardYou ? 1 : -1);
  pgServe = 30;  // a second's pause before it moves
  cpAim = (fr() - 0.5f) * 13;
}
static void pongReset() {
  pgX = cpX = 32;
  pgLives = 3;
  pongServe(true);
}
static bool pongStep(const GameInput &in) {
  t++;
  pgX += (clampf(32 + in.roll * 28, 6, 58) - pgX) * 0.35f;
  // the clock's paddle chases the ball, a little slower than it could
  float cpSpeed = 0.40f + std::min(0.35f, score * 0.025f);
  if (bVY < 0 && bY < 20) cpX += clampf(bX + cpAim - cpX, -cpSpeed, cpSpeed);
  else cpX += clampf(32 - cpX, -0.3f, 0.3f);
  cpX = clampf(cpX, 6, 58);
  if (pgServe > 0) {
    pgServe--;
    return true;
  }
  bX += bVX;
  bY += bVY;
  if (bX < 0.5f) bX = 0.5f, bVX = fabsf(bVX);
  if (bX > 62.5f) bX = 62.5f, bVX = -fabsf(bVX);
  auto bounce = [&](float padX, bool down) {
    float off = clampf((bX - padX) / 6, -1, 1);
    float sp = std::min(2.0f, sqrtf(bVX * bVX + bVY * bVY) * 1.05f);
    float a = off * 1.0f;
    bVX = sp * sinf(a);
    bVY = sp * cosf(a) * (down ? 1 : -1);
  };
  if (bVY > 0 && bY >= 28.5f && bY < 30.5f && fabsf(bX - pgX) <= 6.5f) bounce(pgX, false), bY = 28.5f;
  if (bVY < 0 && bY <= 2.5f && bY > 0.5f && fabsf(bX - cpX) <= 6.5f) {
    bounce(cpX, true), bY = 2.5f;
    cpAim = (fr() - 0.5f) * 13;
  }
  if (bY < -1) {  // got past the clock
    score++;
    pongServe(true);
  } else if (bY > 33) {
    if (--pgLives <= 0) return false;
    pongServe(false);
  }
  return true;
}
static void pongDraw(int frame) {
  gfxBegin();
  for (int x = 0; x < 64; x += 4) px(x, 15, 0x202838), px(x + 1, 15, 0x202838);
  fill((int)lroundf(cpX) - 6, 1, 13, 2, 0xFF6060);
  fill((int)lroundf(pgX) - 6, 29, 13, 2, 0x40C0FF);
  if (pgServe == 0 || (frame / 4) % 2) fill((int)lroundf(bX) - 1, (int)lroundf(bY) - 1, 2, 2, 0xFFFFFF);
  txt(String(score), 1, 12, 0x506070);
  for (int i = 0; i < pgLives; i++) px(62 - i * 2, 13, 0x40C0FF);
  gfxEnd();
}
static GameInput pongAuto() {
  GameInput in;
  float target = 32;
  if (bVY > 0) {  // where it will cross the bottom
    float x = bX, y = bY, vx = bVX;
    for (int i = 0; i < 200 && y < 28.5f; i++) {
      x += vx, y += bVY;
      if (x < 0.5f || x > 62.5f) vx = -vx;
    }
    target = x + (score % 3 - 1) * 2.5f;
  }
  in.roll = clampf((target - 32) / 28, -1, 1);
  return in;
}

// =============================================================== Simon: repeat the colours; tilt picks, tap presses
static uint8_t smSeq[64];
static int smLen = 0, smPos = 0, smPhase = 0, smTimer = 0, smLit = -1, smCursor = 0, smFlash = 0;
static const uint32_t SM_COL[4] = {0xFF2D2D, 0x2DFF5A, 0x2D7DFF, 0xFFD02D};
static void simonNext() {
  smSeq[smLen++] = rnd() % 4;
  smPos = 0;
  smPhase = 0;  // showing
  smTimer = 25;
}
static void simonReset() {
  smLen = 0;
  smCursor = 1;
  simonNext();
}
static bool simonStep(const GameInput &in) {
  t++;
  smCursor = clampf((int)((in.roll + 1) * 2), 0, 3);
  int on = std::max(8, 18 - smLen), off = std::max(4, 7 - smLen / 3);
  if (smPhase == 0) {  // play the sequence back
    if (--smTimer > 0) return true;
    int step = smPos / 2;
    if (step >= smLen) {
      smPhase = 1, smPos = 0, smLit = -1, smTimer = 150;
      return true;
    }
    smLit = smPos % 2 == 0 ? smSeq[step] : -1;
    smTimer = smPos % 2 == 0 ? on : off;
    smPos++;
    return true;
  }
  if (smPhase == 1) {  // your turn: 5 s for each press
    if (smFlash > 0 && --smFlash == 0) smLit = -1;
    if (in.tap) {
      smLit = smCursor;
      smFlash = 8;
      if (smCursor != smSeq[smPos]) return false;
      smTimer = 150;
      if (++smPos >= smLen) {
        score = smLen;
        smPhase = 2, smTimer = 30;
      }
    } else if (--smTimer <= 0) {
      return false;  // too slow
    }
    return true;
  }
  if (--smTimer <= 0) simonNext();  // smPhase 2: well done, a pause before the next round
  return smLen < 64;
}
static void simonDraw(int frame) {
  gfxBegin();
  for (int i = 0; i < 4; i++) {
    bool lit = smLit == i || (smPhase == 2 && (frame / 4) % 2);
    fill(i * 16 + 1, 2, 14, 24, lit ? SM_COL[i] : mix(0x000000, SM_COL[i], 0.18f));
    if (smPhase == 1 && smCursor == i) fill(i * 16 + 1, 28, 14, 2, 0xFFFFFF);
  }
  if (smPhase == 0) txtC(String(smLen), 11, 0xFFFFFF);
  gfxEnd();
}
static GameInput simonAuto() {
  GameInput in;
  if (smPhase != 1) return in;
  int want = smSeq[smPos];
  if (smLen > 7 && (rnd() % 100) < 2) want = (want + 1) % 4;  // the autopilot slips up eventually
  in.roll = -0.75f + want * 0.5f;
  in.tap = smCursor == want && (t % 9 == 0);
  return in;
}

// =============================================================== Lander: lean back to fire the engine, tilt to drift
static uint8_t ldGround[64];
static int ldPad = 28, ldPadW = 9, ldLevel = 0, ldDone = 0;
static float ldX, ldY, ldVX, ldVY, ldFuel;
static bool ldBurn = false;
static void landerLevel() {
  ldPadW = std::max(5, 9 - ldLevel);
  ldPad = 4 + rnd() % (56 - ldPadW);
  float h = 26 + fr() * 3;
  for (int x = 0; x < 64; x++) {
    h = clampf(h + (fr() - 0.5f) * 3.2f, 18, 30);
    ldGround[x] = (uint8_t)h;
  }
  int padY = 22 + rnd() % 7;
  for (int x = ldPad; x < ldPad + ldPadW; x++) ldGround[x] = padY;
  ldX = ldPad < 32 ? 54 : 8;
  ldY = 3;
  ldVX = ldPad < 32 ? -0.15f : 0.15f;
  ldVY = 0;
  ldFuel = 100;
  ldDone = 0;
}
static void landerReset() {
  ldLevel = 0;
  landerLevel();
}
static bool landerStep(const GameInput &in) {
  t++;
  if (ldDone) {  // landed: a moment to enjoy it, then the next pad
    if (--ldDone == 0) ldLevel++, landerLevel();
    return true;
  }
  float g = 0.010f + ldLevel * 0.0015f;
  ldBurn = (in.pitch > 0.25f || in.tap) && ldFuel > 0;
  if (ldBurn) ldVY -= 0.024f, ldFuel -= 0.45f;
  if (fabsf(in.roll) > 0.15f && ldFuel > 0) ldVX += in.roll * 0.006f, ldFuel -= 0.1f;
  ldVY += g;
  ldX += ldVX;
  ldY += ldVY;
  if (ldX < 1) ldX = 1, ldVX = 0;
  if (ldX > 62) ldX = 62, ldVX = 0;
  if (ldY < 1) ldY = 1, ldVY = std::max(0.0f, ldVY);
  int gx = (int)lroundf(ldX);
  if (ldY + 1.5f >= std::min(ldGround[std::max(0, gx - 1)], std::min(ldGround[gx], ldGround[std::min(63, gx + 1)]))) {
    bool onPad = gx - 1 >= ldPad && gx + 1 < ldPad + ldPadW;
    if (onPad && ldVY < 0.32f && fabsf(ldVX) < 0.22f) {
      score += 50 + (int)ldFuel / 2;
      ldDone = 45;
      ldVX = ldVY = 0;
      return true;
    }
    return false;  // crashed
  }
  return true;
}
static void landerDraw(int frame) {
  gfxBegin();
  for (int i = 0; i < 14; i++) px((i * 37 + 11) % 64, (i * 23 + 5) % 16, 0x404060);
  for (int x = 0; x < 64; x++) {
    fill(x, ldGround[x], 1, 32 - ldGround[x], 0x3A3A44);
    px(x, ldGround[x], 0x70707C);
  }
  bool padLit = (frame / 6) % 2 || ldDone;
  fill(ldPad, ldGround[ldPad], ldPadW, 1, padLit ? 0xFFD000 : 0x806800);
  int x = (int)lroundf(ldX), y = (int)lroundf(ldY);
  fill(x - 1, y - 1, 3, 2, 0xE0E0E0);
  px(x - 1, y + 1, 0xA0A0A0), px(x + 1, y + 1, 0xA0A0A0);
  if (ldBurn && !ldDone) px(x, y + 1 + (frame % 2), (frame / 2) % 2 ? 0xFF8000 : 0xFFE000);
  // fuel and speed
  fill(0, 0, (int)(ldFuel * 0.2f), 1, ldFuel > 25 ? 0x30C0FF : 0xFF4040);
  bool safe = ldVY < 0.32f && fabsf(ldVX) < 0.22f;
  px(63, 0, safe ? 0x40FF60 : 0xFF3030);
  if (ldDone) txtC("LANDED", 8, 0x40FF60);
  txt(String(score), 1, 2, 0x707080);
  gfxEnd();
}
static GameInput landerAuto() {
  GameInput in;
  float padX = ldPad + ldPadW / 2.0f;
  float wantVX = clampf((padX - ldX) * 0.02f, -0.18f, 0.18f);
  in.roll = clampf((wantVX - ldVX) * 12, -1, 1);
  float h = ldGround[(int)lroundf(ldX)] - ldY;
  float wantVY = fabsf(padX - ldX) > 4 ? (h > 12 ? 0.05f : -0.05f) : std::min(0.25f, 0.06f + h * 0.012f);
  in.pitch = ldVY > wantVY ? 1 : 0;
  return in;
}

// =============================================================== Reaction: wait for green, then tap
static int rxRound = 0, rxState = 0, rxTimer = 0, rxTotal = 0, rxLast = 0;
static const int RX_ROUNDS = 5;
static void reactionReset() {
  rxRound = 0;
  rxTotal = 0;
  rxState = 0;
  rxTimer = 45 + rnd() % 75;  // 1.5 - 4 s of red
}
static bool reactionStep(const GameInput &in) {
  t++;
  switch (rxState) {
    case 0:  // red: wait
      if (in.tap) {
        rxLast = -1;  // too soon: counts as a full second
        rxTotal += 1000;
        rxState = 2, rxTimer = 40;
      } else if (--rxTimer <= 0) {
        rxState = 1, rxTimer = 0;
      }
      break;
    case 1:  // green: count frames until the tap
      rxTimer++;
      if (in.tap || rxTimer > 60) {
        rxLast = in.tap ? (int)lroundf(rxTimer * 33.3f) : 2000;
        rxTotal += std::min(rxLast, 1000);
        rxState = 2, rxTimer = 40;
      }
      break;
    case 2:  // show the time
      if (--rxTimer <= 0) {
        if (++rxRound >= RX_ROUNDS) rxState = 3, rxTimer = 75;
        else rxState = 0, rxTimer = 45 + rnd() % 75;
      }
      break;
    default:  // the average, then game over
      score = std::max(0, 1000 - rxTotal / RX_ROUNDS);
      if (--rxTimer <= 0) return false;
  }
  return true;
}
static void reactionDraw(int frame) {
  gfxBegin();
  if (rxState == 0) {
    fill(0, 0, 64, 32, 0x400000);
    txtC("WAIT", 8, 0xFF4040);
    txtC(String(rxRound + 1) + "/" + String(RX_ROUNDS), 20, 0x803030);
  } else if (rxState == 1) {
    fill(0, 0, 64, 32, 0x00A030);
    txtC("TAP!", 12, 0xFFFFFF);
  } else if (rxState == 2) {
    if (rxLast < 0) txtC("TOO SOON", 12, 0xFF6040);
    else txtC(String(rxLast) + " MS", 12, rxLast < 300 ? 0x40FF60 : rxLast < 450 ? 0xFFD000 : 0xFF6040);
  } else {
    txtC("AVERAGE", 4, 0xA0A0A0);
    txtC(String(rxTotal / RX_ROUNDS) + " MS", 16, 0xFFFFFF);
  }
  (void)frame;
  gfxEnd();
}
static GameInput reactionAuto() {
  GameInput in;
  in.tap = rxState == 1 && rxTimer >= 7 + (int)(rxRound * 3 % 4);
  return in;
}

// =============================================================== dispatch
void gameReset(int id, uint32_t seed) {
  rs = seed ? seed : 1;
  score = 0;
  t = 0;
  switch (id) {
    case G_DODGER: dodgerReset(); break;
    case G_BIRD: birdReset(); break;
    case G_MARBLE: marbleReset(); break;
    case G_CATCH: catchReset(); break;
    case G_BREAKOUT: breakoutReset(); break;
    case G_RACER: racerReset(); break;
    case G_PONG: pongReset(); break;
    case G_SIMON: simonReset(); break;
    case G_LANDER: landerReset(); break;
    case G_REACTION: reactionReset(); break;
    default: balanceReset(); break;
  }
}
bool gameStep(int id, const GameInput &in) {
  switch (id) {
    case G_DODGER: return dodgerStep(in);
    case G_BIRD: return birdStep(in);
    case G_MARBLE: return marbleStep(in);
    case G_CATCH: return catchStep(in);
    case G_BREAKOUT: return breakoutStep(in);
    case G_RACER: return racerStep(in);
    case G_PONG: return pongStep(in);
    case G_SIMON: return simonStep(in);
    case G_LANDER: return landerStep(in);
    case G_REACTION: return reactionStep(in);
    default: return balanceStep(in);
  }
}
void gameDraw(int id, int frame) {
  switch (id) {
    case G_DODGER: dodgerDraw(frame); break;
    case G_BIRD: birdDraw(frame); break;
    case G_MARBLE: marbleDraw(frame); break;
    case G_CATCH: catchDraw(frame); break;
    case G_BREAKOUT: breakoutDraw(frame); break;
    case G_RACER: racerDraw(frame); break;
    case G_PONG: pongDraw(frame); break;
    case G_SIMON: simonDraw(frame); break;
    case G_LANDER: landerDraw(frame); break;
    case G_REACTION: reactionDraw(frame); break;
    default: balanceDraw(frame); break;
  }
}
int gameScore(int) { return score; }
GameInput gameAutopilot(int id) {
  switch (id) {
    case G_DODGER: return dodgerAuto();
    case G_BIRD: return birdAuto();
    case G_MARBLE: return marbleAuto();
    case G_CATCH: return catchAuto();
    case G_BREAKOUT: return breakoutAuto();
    case G_RACER: return racerAuto();
    case G_PONG: return pongAuto();
    case G_SIMON: return simonAuto();
    case G_LANDER: return landerAuto();
    case G_REACTION: return reactionAuto();
    default: return balanceAuto();
  }
}

// =============================================================== menu / ready / game over
void drawGameMenu(int sel, int best, int frame) {
  gfxBegin();
  txtC("TILT < >", 0, 0x9CAAB6);
  txtC(gameName(sel), 11, 0xFFD16A);
  String record = best > 0 ? "BEST " + String(best) : String();
  txtC((frame / 45) % 2 && record.length() <= 10 && record.length() ? record : String("TAP=PLAY"), 23, 0x80D7FF);
  gfxEnd();
}
void drawGameReady(int id, int frame) {
  gfxBegin();
  txtC(gameName(id), 4, 0xFFC000);
  txtC(gameHint(id), 18, (frame / 8) % 2 ? 0xFFFFFF : 0x909090);
  gfxEnd();
}
void drawGameOver(int id, int sc, int best, bool newBest, int frame) {
  gfxBegin();
  txtC("GAME OVER", 1, 0xFF4040);
  txtC(String(sc), 11, 0xFFFFFF);
  if (newBest) txtC("NEW BEST!", 23, (frame / 5) % 2 ? 0xFFD700 : 0xFF8C00);
  else txtC("BEST " + String(best), 23, 0x40C0FF);
  (void)id;
  gfxEnd();
}
