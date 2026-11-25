// render5d.cpp - batch 5 data screens: launches, playoff race, park waits, stocks, news, last play, drive time,
// Powerball, pick'em. Each draw function paints one whole frame (gfxBegin clears the back buffer, so even the news
// banner is a full frame: the banner slides over a dimmed team-colour backdrop).
#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include "feeds5.h"
#include "gfxutil.h"
#include "model.h"
#include "render.h"
#include "render5.h"

extern Settings CFG;
using namespace gu;

namespace {
const uint32_t GREEN = 0x30E070, RED = 0xFF4040, AMBER = 0xFFB020, GREY = 0x909090, DIM = 0x505050;

String up(String s) { s = foldAscii(s); s.toUpperCase(); return s; }

// keep only what the 3x5 font can draw
String clean3(const String &s0) {
  String s = up(s0), o;
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || strchr(" :.,-+/%&#<>@", c)) o += c;
    else if (c == '\'' || c == '"' || c == '!' || c == '?') continue;
    else o += ' ';
  }
  String r;
  for (unsigned i = 0; i < o.length(); i++)
    if (!(o[i] == ' ' && (r.length() == 0 || r[r.length() - 1] == ' '))) r += o[i];
  while (r.length() && r[r.length() - 1] == ' ') r.remove(r.length() - 1);
  return r;
}
String fit3(String s, int maxPx) {
  while (s.length() && text3Width(s) > maxPx) s.remove(s.length() - 1);
  while (s.length() && s[s.length() - 1] == ' ') s.remove(s.length() - 1);
  return s;
}
int text8W(const String &s) { return txtW(s); }

// scrolling ticker in the window [x, x+w); draw it BEFORE anything else that sits beside it (margins are masked
// with bg). Short text is just drawn at x.
void ticker(const String &s, int x, int y, int w, uint32_t c, int scrollX, bool big, uint32_t bg = 0) {
  int tw = big ? text8W(s) : text3Width(s), h = big ? 8 : 5;
  if (tw <= w) {
    if (big) txt(s, x, y, c); else text3(s, x, y, c);
    return;
  }
  int gap = big ? 24 : 16, off = ((scrollX % (tw + gap)) + (tw + gap)) % (tw + gap);
  for (int k = 0; k < 2; k++) {
    int xx = x - off + k * (tw + gap);
    if (big) txt(s, xx, y, c); else text3(s, xx, y, c);
  }
  if (x > 0) fill(0, y, x, h, bg);
  if (x + w < sw()) fill(x + w, y, sw() - x - w, h, bg);
}

String clock12(time_t t) {
  struct tm tm;
  localtime_r(&t, &tm);
  char b[12];
  if (CFG.twelveHour) {
    int h = tm.tm_hour % 12; if (!h) h = 12;
    snprintf(b, sizeof b, "%d:%02d%c", h, tm.tm_min, tm.tm_hour < 12 ? 'A' : 'P');
  } else snprintf(b, sizeof b, "%02d:%02d", tm.tm_hour, tm.tm_min);
  return b;
}
const char *DOW[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
String dowOf(time_t t) { struct tm tm; localtime_r(&t, &tm); return DOW[tm.tm_wday]; }

void bitmap(int x, int y, const char *const *rows, int n, uint32_t (*pal)(char)) {
  for (int j = 0; j < n; j++)
    for (int i = 0; rows[j][i]; i++) {
      uint32_t c = pal(rows[j][i]);
      if (c) px(x + i, y + j, c);
    }
}

// ---- launch
uint32_t statusColor(const String &st) {
  String s = up(st);
  if (s.startsWith("GO") || s.startsWith("SUCCESS")) return GREEN;
  if (s.startsWith("TB")) return AMBER;
  if (s.startsWith("HOLD") || s.startsWith("FAIL")) return RED;
  return GREY;
}
uint32_t rocketPal(char c) {
  switch (c) {
    case 'w': return 0xE8E8F0;
    case 'r': return 0xE03030;
    case 'b': return 0x40A0FF;
    case 'f': return 0xC04040;
    default: return 0;
  }
}
const char *ROCKET[] = {"..rr..", ".wwww.", ".wbbw.", ".wwww.", ".wwww.", ".wwww.", "fwwwwf", "f.ww.f"};

// ---- park names
String shortRide(const String &n0) {
  String s = clean3(n0);
  struct R { const char *a, *b; };
  static const R rep[] = {{"THE ", ""},      {"MOUNTAIN", "MTN"}, {"ADVENTURES", "ADV"}, {"ADVENTURE", "ADV"},
                          {"RAILROAD", "RR"}, {"EXPERIENCE", "EXP"}, {"SPACE ", "SP "},   {"STAR WARS", "SW"},
                          {"ROCKIN ", ""},    {"ROLLER COASTER", "COASTER"}, {"RIDE", ""}, {"SPINNER", "SPIN"},
                          {"MR TOAD'S", "TOAD"}, {"PIRATES OF THE CARIBBEAN", "PIRATES"}, {"DISNEYLAND", "DL"},
                          {"AND ", "& "},     {"CALIFORNIA", "CA"}, {"SCREAMIN", "SCREAM"}, {"INDIANA JONES", "INDY"}};
  for (auto &r : rep) {
    String a = r.a;
    int i;
    while ((i = s.indexOf(a)) >= 0 && (a.length() > 4 || s.length() > 12)) s = s.substring(0, i) + r.b + s.substring(i + a.length());
  }
  while (s.indexOf("  ") >= 0) s.replace("  ", " ");
  s.trim();
  return s;
}
}  // namespace

// ================================================================ launch
void drawLaunch(const Launch &l, time_t now, int frame) {
  gfxBegin();
  long dt = (long)(l.net - now);
  bool lifted = l.net && dt <= 0;
  uint32_t sc = statusColor(l.status);
  // status tag (top right)
  String st = clean3(l.status.length() ? l.status : "TBD");
  if (st.length() > 5) st = st.substring(0, 5);
  int tagW = text3Width(st) + 3;
  String top = clean3(l.mission.length() ? l.rocket + " - " + l.mission : l.name.length() ? l.name : l.rocket);
  ticker(top, 1, 1, 64 - tagW - 4, 0xE0E0E0, frame / 3, false);
  fill(64 - tagW, 0, tagW, 7, sc);
  text3(st, 64 - tagW + 2, 1, 0x000000);
  // countdown
  char b[24];
  uint32_t cc = sc;
  if (!l.net) {
    snprintf(b, sizeof b, "T-??:??:??");
  } else if (!lifted) {
    long d = dt / 86400, h = dt % 86400 / 3600, m = dt % 3600 / 60, s = dt % 60;
    if (d > 0) snprintf(b, sizeof b, "T-%ldD %02ld:%02ld", d, h, m);
    else snprintf(b, sizeof b, "T-%02ld:%02ld:%02ld", h, m, s);
    if (dt < 600) cc = (frame / 8 & 1) || dt > 60 ? 0xFFFFFF : sc;
  } else {
    long e = -dt;
    if (e < 8) {
      snprintf(b, sizeof b, "LIFTOFF");
      cc = (frame / 4 & 1) ? 0xFFE060 : 0xFF8030;
    } else if (e < 3600) snprintf(b, sizeof b, "T+%02ld:%02ld", e / 60, e % 60);
    else snprintf(b, sizeof b, "T+%ldH %02ldM", e / 3600, e % 3600 / 60);
    if (e >= 8) cc = GREEN;
  }
  String cs = b;
  // dim shadow + bright text, centred
  txtC(cs, 9, cc);
  String pad = clean3(l.pad.length() ? l.pad : l.provider);
  ticker(pad, 1, 20, 54, GREY, frame / 3, false);
  // rocket (bottom right), rising after liftoff (drawn after the tickers, whose margin masks would erase it)
  int rx = 57, ry = 19;
  if (lifted) ry -= (int)std::min<long>(-dt * 2, 14);
  else if (dt < 60) ry += (frame / 2 & 1) ? 0 : 0;
  bitmap(rx, ry, ROCKET, 8, rocketPal);
  if (!lifted && dt > 3600 * 6) ry += 0;
  bool burn = lifted || dt < 20;
  if (burn || (frame / 3 & 1) == 0 || true) {
    int fl = burn ? 3 + (int)(hash32(frame) % 3) : 2;
    if (!burn) fl = 1;
    for (int i = 0; i < fl; i++) {
      uint32_t c = i == 0 ? 0xFFF080 : i == 1 ? 0xFFA020 : 0xE05010;
      if (!burn) c = scale(c, 0.55f);
      px(rx + 2 + (i == 2 ? (frame & 1) : 0), ry + 8 + i, c);
      px(rx + 3 - (i == 2 ? (frame & 1) : 0), ry + 8 + i, c);
      if (i == 0 && burn) { px(rx + 1, ry + 8, 0xFFA020); px(rx + 4, ry + 8, 0xFFA020); }
    }
  }
  // pad line + local time / Vandenberg tag, windows leave room for the rocket
  bool lookUp = l.vandenberg && (frame / 45 & 1);
  if (lookUp) {
    uint32_t hc = (frame / 6 & 1) ? 0xFFE060 : 0xFFB020;
    fill(0, 26, 56, 6, scale(0xFF8000, 0.35f));
    text3("VSFB - LOOK UP", 1, 27, hc);
  } else if (l.net) {
    String when = dowOf(l.net) + " " + clock12(l.net);
    text3(when, 1, 27, 0xA0C0FF);
  }
  gfxEnd();
}

// ================================================================ playoff race
void drawPlayoff(const StandGroup &g, const String &team, uint32_t color, int frame) {
  PlayoffLine pl = playoffLine(g, team);
  gfxBegin();
  if (color < 0x202020) color = 0xA0A0A0;
  fill(0, 0, 34, 2, color);
  txt(team, 0, 3, color);
  if (!pl.ok) {
    t3C("NO DATA", 20, GREY);
    gfxEnd();
    return;
  }
  text3(pl.record, txtW(team) + 4, 5, 0xE0E0E0);
  String seed = pl.seed > 0 ? "#" + String(pl.seed) + " SEED" : "DIV #" + String(pl.rank);
  text3(seed, 0, 12, 0xC0C0C0);
  // big number
  if (pl.rank == 1 && pl.magic == 0) {
    text3("CLINCHED", 0, 19, GREEN);
    uint32_t c = (frame / 10 & 1) ? 0xFFE060 : 0xFFB020;
    txt("***", 0, 24, c);
  } else if (pl.rank == 1 && pl.magic > 0) {
    text3("MAGIC", 0, 19, AMBER);
    gfxText(String(pl.magic), 1, 24, 0xFFD040, 1);
    text3("WINS", 15, 26, 0x707070);
  } else {
    text3(pl.rank == 1 ? "LEADS" : "GB", 0, 19, GREY);
    String gb = pl.gb.length() ? pl.gb : String("-");
    txt(gb, 1, 24, pl.rank == 1 ? GREEN : 0xFFFFFF);
  }
  // mini table on the right: top 4 (ours replaces the 4th when we're lower)
  fill(35, 0, 1, 32, 0x202020);
  int ours = -1;
  for (unsigned i = 0; i < g.rows.size(); i++)
    if (g.rows[i].abbr == team) ours = i;
  int n = std::min<int>(4, g.rows.size());
  for (int i = 0; i < n; i++) {
    int idx = i;
    if (i == 3 && ours > 3) idx = ours;
    const StandRow &r = g.rows[idx];
    int y = 3 + i * 6;
    bool me = r.abbr == team;
    if (me) fill(37, y - 1, 27, 7, scale(color, 0.35f));
    text3(String(idx + 1), 38, y, me ? 0xFFFFFF : DIM + 0x202020);
    text3(r.abbr, 43, y, me ? 0xFFFFFF : 0xB0B0B0);
    String w = String(r.wins);
    t3R(w, 63, y, me ? 0xFFFFFF : 0x909090);
  }
  String gn = clean3(g.name);
  gn.replace("EAST", "E"); gn.replace("WEST", "W"); gn.replace("NORTH", "N"); gn.replace("SOUTH", "S");
  gn.replace("CENTRAL", "C"); gn.replace("DIVISION", "");
  text3(fit3(gn, 26), 37, 27, 0x6080C0);
  gfxEnd();
}

// ================================================================ park waits
int parkPages(const ParkWaits &p) { return std::max<int>(1, ((int)p.rides.size() + 3) / 4); }

void drawParkWaits(const ParkWaits &p, int page, int frame) {
  gfxBegin();
  int pages = parkPages(p);
  page = ((page % pages) + pages) % pages;
  String head = clean3(p.park) + " WAITS";
  String pg = pages > 1 ? String(page + 1) + "/" + String(pages) : String("");
  String pname = fit3(clean3(p.park), 63 - 24);
  head = pname + " WAITS";
  text3(head, 0, 0, 0xFFD040);
  if (text3Width(head) + 28 < 64) t3R("Q-T.COM", 63, 0, DIM);
  fill(0, 6, 64, 1, 0x303030);
  if (pages > 1) {  // page dots on the rule
    int w = pages * 5 - 1, x0 = 64 - w;
    for (int i = 0; i < pages; i++) fill(x0 + i * 5, 6, 4, 1, i == page ? 0xFFFFFF : 0x606060);
  }
  if (!p.ok || p.rides.empty()) {
    t3C(p.ok ? "ALL CLOSED" : "NO DATA", 18, GREY);
    gfxEnd();
    return;
  }
  for (int i = 0; i < 4; i++) {
    unsigned idx = page * 4 + i;
    if (idx >= p.rides.size()) break;
    const RideWait &r = p.rides[idx];
    int y = 9 + i * 6;
    uint32_t c = r.wait >= 60 ? RED : r.wait >= 30 ? AMBER : GREEN;
    fill(0, y, 1, 5, c);
    text3(fit3(shortRide(r.name), 43), 3, y, 0xD8D8D8);
    // wait bar behind the number (max 90 min)
    String w = String(r.wait);
    t3R(w, 56, y, c);
    text3("M", 60, y, scale(c, 0.55f));
  }
  gfxEnd();
}

// ================================================================ stock
void drawStock(const StockQuote &q, int frame) {
  gfxBegin();
  float ref = q.prevClose > 0 ? q.prevClose : q.open;
  float chg = ref > 0 ? q.price - ref : 0, pct = ref > 0 ? chg / ref * 100 : 0;
  bool upd = chg >= 0;
  uint32_t col = ref <= 0 ? 0xC0C0C0 : upd ? GREEN : RED;
  String sym = up(q.symbol);
  char pb[16];
  snprintf(pb, sizeof pb, q.price >= 1000 ? "%.1f" : "%.2f", q.price);
  String price = pb;
  bool bigSym = txtW(sym) + txtW(price) + 4 <= 64;
  if (bigSym) txt(sym, 0, 0, 0xE0E0FF);
  else text3(sym, 0, 1, 0xE0E0FF);
  txtR(price, 63, 0, 0xFFFFFF);
  int sx = (bigSym ? txtW(sym) : text3Width(sym)) + 3;
  if (q.halted) {
    int w = text3Width("HALT") + 3;
    if (sx + w < 64 - txtW(price)) {
      fill(sx, 1, w, 7, (frame / 10 & 1) ? RED : scale(RED, 0.6f));
      text3("HALT", sx + 2, 2, 0xFFFFFF);
    }
  }
  // change row
  if (ref > 0) {
    int ax = 1, ay = 11;
    for (int i = 0; i < 4; i++) {  // triangle
      int w = upd ? i * 2 + 1 : (3 - i) * 2 + 1;
      fill(ax + 3 - w / 2, ay + i, w, 1, col);
    }
    char cb[16];
    snprintf(cb, sizeof cb, "%+.1f%%", pct);
    txt(cb, 9, 9, col);
    snprintf(cb, sizeof cb, "%+.2f", chg);
    t3R(cb, 63, 11, scale(col, 0.85f));
  } else {
    text3("NO PREV CLOSE", 1, 11, GREY);
  }
  // day range bar
  float lo = q.dayLow, hi = q.dayHigh;
  if (hi <= lo) { lo = std::min(lo > 0 ? lo : q.price, q.price); hi = std::max(hi, q.price); }
  char lb[12], hb[12];
  snprintf(lb, sizeof lb, "%.0f", lo);
  snprintf(hb, sizeof hb, "%.0f", hi);
  int lw = text3Width(lb), hw = text3Width(hb);
  text3(lb, 0, 19, 0x808080);
  t3R(hb, 63, 19, 0x808080);
  int bx = lw + 3, bw = 64 - hw - 3 - bx;
  if (bw > 6) {
    fill(bx, 21, bw, 1, 0x404040);
    for (int i = 0; i <= 2; i++) px(bx + bw * i / 2, 20, 0x404040);
    if (hi > lo) {
      int mx = bx + (int)lroundf((q.price - lo) / (hi - lo) * (bw - 1));
      mx = std::max(bx, std::min(bx + bw - 1, mx));
      fill(bx, 21, mx - bx + 1, 1, scale(col, 0.6f));
      fill(mx, 19, 1, 5, 0xFFFFFF);
    }
  }
  // sparkline
  const int top = 26, bot = 31;
  if (q.spark.size() >= 2) {
    float mn = q.spark[0], mxv = q.spark[0];
    for (float v : q.spark) mn = std::min(mn, v), mxv = std::max(mxv, v);
    if (ref > 0) mn = std::min(mn, ref), mxv = std::max(mxv, ref);
    float span = std::max(mxv - mn, 0.0001f);
    int n = q.spark.size(), py = -1;
    auto yOf = [&](float v) { return bot - (int)lroundf((v - mn) / span * (bot - top)); };
    if (ref > 0) for (int x = 0; x < 64; x += 3) px(x, yOf(ref), 0x303030);
    for (int x = 0; x < 64; x++) {
      int i = (int)((long)x * (n - 1) / 63);
      int y = yOf(q.spark[i]);
      for (int yy = y + 1; yy <= bot; yy++) px(x, yy, scale(col, 0.18f));
      if (py >= 0) line(x - 1, py, x, y, col); else px(x, y, col);
      py = y;
    }
    px(63, py, 0xFFFFFF);
  } else {
    for (int x = 0; x < 64; x += 2) px(x, 29, 0x303030);
  }
  gfxEnd();
}

// ================================================================ news
void drawNewsBanner(const NewsItem &n, int frame, int scrollX) {
  gfxBegin();
  const int H = 17;
  int f = std::min(frame, 6);
  float e = 1 - powf(1 - f / 6.0f, 2);
  int y0 = (int)lroundf(-H + H * e);   // slides down; top row is y0
  uint32_t col = n.color < 0x202020 ? 0xFFFFFF : n.color;
  // dim backdrop below the banner: a glow of the team colour
  for (int y = 0; y < 32; y++) fill(0, y, 64, 1, scale(col, 0.10f * (1 - y / 40.0f)));
  String who = clean3(n.why.length() ? n.why : n.league);
  if (f >= 6) txtC(who, 21, scale(col, 0.5f));
  // banner body
  uint32_t bg = scale(col, 0.22f);
  fill(0, y0, 64, H, bg);
  ticker(clean3(n.headline), 4, y0 + 8, 60, 0xFFFFFF, scrollX, true, bg);
  fill(0, y0, 3, H, col);
  fill(0, y0 + H - 1, 64, 1, col);
  String lg = clean3(n.league);
  String tag = frame / 12 % 2 ? lg : String("BREAKING");
  if (!lg.length()) tag = "BREAKING";
  text3(tag, 5, y0 + 1, col == 0xFFFFFF ? 0xFFD040 : mix(col, 0xFFFFFF, 0.5f));
  if (n.why.length() && tag != who) t3R(fit3(who, 28), 63, y0 + 1, 0xC0C0C0);
  gfxEnd();
}

void drawNewsList(const std::vector<NewsItem> &items, int sel, int scrollX) {
  gfxBegin();
  text3("ALERTS", 1, 0, 0xFFD040);
  t3R(String((int)items.size()), 63, 0, GREY);
  fill(0, 6, 64, 1, 0x303030);
  if (items.empty()) {
    t3C("ALL QUIET", 18, GREY);
    gfxEnd();
    return;
  }
  int n = items.size();
  sel = std::max(0, std::min(n - 1, sel));
  int first = std::max(0, std::min(sel - 1, n - 4));
  time_t now = time(nullptr);
  for (int i = 0; i < 4 && first + i < n; i++) {
    int idx = first + i, y = 8 + i * 6;
    const NewsItem &it = items[idx];
    uint32_t col = it.color < 0x202020 ? 0xFFFFFF : it.color;
    bool s = idx == sel;
    long age = it.published ? (long)(now - it.published) : 0;
    char ab[8];
    if (age < 3600) snprintf(ab, sizeof ab, "%ldM", std::max(0L, age / 60));
    else if (age < 86400) snprintf(ab, sizeof ab, "%ldH", age / 3600);
    else snprintf(ab, sizeof ab, "%ldD", age / 86400);
    String h = clean3(it.headline);
    if (s) {
      fill(0, y - 1, 64, 7, scale(col, 0.25f));
      ticker(h, 16, y, 47, 0xFFFFFF, scrollX, false, scale(col, 0.25f));
      fill(0, y - 1, 1, 7, col);
    } else {
      ticker(fit3(h, 47), 16, y, 47, 0x909090, 0, false);
    }
    text3(ab, 2, y, s ? 0xFFFFFF : scale(col, 0.8f));
    px(14, y + 2, s ? col : scale(col, 0.4f));
  }
  gfxEnd();
}

// ================================================================ last play
void drawLastPlay(const Game &g, int scrollX) {
  gfxBegin();
  uint32_t ac = teamColor(g.away), hc = teamColor(g.home);
  if (ac == hc) hc = 0xFFFFFF;
  fill(0, 0, 2, 9, ac);
  fill(62, 0, 2, 9, hc);
  text3(clean3(g.away.abbr), 4, 2, ac);
  t3R(clean3(g.home.abbr), 59, 2, hc);
  txt(String(g.away.score), 4 + text3Width(clean3(g.away.abbr)) + 3, 0, 0xFFFFFF);
  txtR(String(g.home.score), 59 - text3Width(clean3(g.home.abbr)) - 3, 0, 0xFFFFFF);
  // possession dot under the abbreviation
  bool awayBall = g.poss.length() && g.poss == g.away.id, homeBall = g.poss.length() && g.poss == g.home.id;
  if (awayBall) fill(4, 10, 5, 1, 0xC08040), fill(5, 9, 3, 1, 0xC08040);
  if (homeBall) fill(55, 10, 5, 1, 0xC08040), fill(56, 9, 3, 1, 0xC08040);
  String det = clean3(g.detail);
  t3C(det, 11, 0xB0B0B0);
  bool nfl = g.league == "nfl" && g.down.length();
  if (nfl) {
    String dd = clean3(g.down);
    dd.replace(" & ", "&");
    String sp = clean3(g.spot);
    String line = sp.length() ? dd + " @" + sp : dd;
    t3C(fit3(line, 62), 18, g.redzone ? RED : 0xFFD040);
  } else if (g.league == "mlb") {
    String c = String(g.balls) + "-" + String(g.strikes) + "  " + String(g.outs) + " OUT";
    t3C(c, 18, 0xFFD040);
  }
  // last play ticker at the bottom, 6x8
  String lp = g.lastPlay.length() ? foldAscii(g.lastPlay) : String("NO PLAY YET");
  fill(0, 24, 64, 8, 0x101820);
  ticker(lp, 1, 24, 62, 0xFFFFFF, scrollX, true, 0x101820);
  gfxEnd();
}

// ================================================================ drive time
void drawDriveTime(const DriveTime &d, time_t now, int frame) {
  gfxBegin();
  if (!d.ok) {
    t3C("NO ROUTE DATA", 14, GREY);
    gfxEnd();
    return;
  }
  float ratio = d.typical > 0 ? (float)d.minutes / d.typical : 1.0f;
  uint32_t col = ratio < 1.15f ? GREEN : ratio < 1.4f ? AMBER : RED;
  // car
  static const char *CAR[] = {"....cccccc....", "...cwwcwwwc...", ".cccccccccccc.", "cccccccccccccc", "cccccccccccccc", ".kkk......kkk.", ".kkk......kkk."};
  int cy = 1;
  for (int j = 0; j < 7; j++)
    for (int i = 0; i < 14; i++) {
      char c = CAR[j][i];
      uint32_t p = c == 'c' ? col : c == 'w' ? 0xA0D0FF : c == 'k' ? 0x707070 : 0;
      if (p) px(1 + i, cy + j, p);
    }
  px(15, cy + 4, 0xFFF0A0);
  px(0, cy + 4, (frame / 6 & 1) && ratio >= 1.4f ? 0xFF2020 : scale(0xFF2020, 0.5f));
  text3(fit3(clean3(d.label.length() ? d.label : "DRIVE"), 40), 20, 3, 0xE0E0E0);
  // big minutes
  String m = String(d.minutes);
  gfxText(m, 2, 8, col, 2);
  int mw = m.length() * 12;
  text3("MIN", 4 + mw, 17, scale(col, 0.8f));
  // typical
  t3R("TYPICAL", 63, 9, 0x808080);
  t3R(String(d.typical) + " MIN", 63, 16, 0xC0C0C0);
  // bar: normal portion then delay
  int total = std::max(d.minutes, d.typical), bw = 62;
  int tw = total ? (int)lroundf((float)std::min(d.minutes, d.typical) / total * bw) : bw;
  fill(1, 25, bw, 1, 0x202020);
  fill(1, 25, tw, 1, scale(GREEN, 0.7f));
  if (d.minutes > d.typical) fill(1 + tw, 25, bw - tw, 1, col);
  String msg = d.delay > 0 ? "+" + String(d.delay) + " MIN TRAFFIC" : String("CLEAR ROADS");
  t3C(msg, 27, col);
  gfxEnd();
}

// ================================================================ Powerball
void drawLotto(const LottoDraw &d, time_t now, int frame) {
  gfxBegin();
  if (!d.ok) {
    t3C("NO DRAW DATA", 14, GREY);
    gfxEnd();
    return;
  }
  for (int i = 0; i < 6; i++) {
    bool pb = i == 5;
    int num = pb ? d.power : d.white[i];
    float cx = 5.0f + i * 10.4f + (pb ? 1 : 0), cy = 5.0f;
    disc(cx, cy, 4.7f, pb ? 0xC81414 : 0x3050B0);
    if (!pb) { px((int)cx - 2, 1, 0x90B0FF); px((int)cx - 1, 0, 0x90B0FF); }
    char b[4];
    snprintf(b, sizeof b, "%d", num);
    String s = b;
    int w = text3Width(s);
    text3(s, (int)lroundf(cx - w / 2.0f - 0.2f + (s.length() == 1 ? 0 : 0)), 3, 0xFFFFFF);
  }
  // sparkle on a ball
  int sp = (frame / 5) % 8;
  if (sp < 6) {
    int sx = (int)(5 + sp * 10.4f) - 2, sy = 2;
    px(sx, sy, 0xFFFFFF);
  }
  text3("JACKPOT", 1, 12, 0xFFB020);
  if (d.multiplier > 0) t3R("PP " + String(d.multiplier) + "X", 63, 12, 0x60C0FF);
  String j = up(d.jackpot);
  j.replace(" MILLION", "M");
  j.replace(" BILLION", "B");
  if (j.length()) {
    if (txtW(j) <= 64) txtC(j, 18, 0xFFD040 + ((frame / 15 & 1) ? 0 : 0));
    else t3C(clean3(j), 20, 0xFFD040);
  } else {
    struct tm tm;
    gmtime_r(&d.drawDate, &tm);
    char b[16];
    snprintf(b, sizeof b, "%s %d/%d", DOW[tm.tm_wday], tm.tm_mon + 1, tm.tm_mday);
    txtC(b, 18, 0xE0E0E0);
  }
  // countdown to next draw
  long dt = d.nextDraw ? (long)(d.nextDraw - now) : 0;
  char nb[32];
  if (!d.nextDraw) nb[0] = 0;
  else if (dt <= 0) snprintf(nb, sizeof nb, "DRAWING NOW");
  else if (dt >= 86400) snprintf(nb, sizeof nb, "NEXT %ldD %ldH", dt / 86400, dt % 86400 / 3600);
  else snprintf(nb, sizeof nb, "NEXT %02ld:%02ld:%02ld", dt / 3600, dt % 3600 / 60, dt % 60);
  if (nb[0]) t3C(nb, 27, 0x90B0FF);
  gfxEnd();
}

// ================================================================ pick'em
int pickemPages(const std::vector<PickemPick> &p) { return std::max<int>(1, ((int)p.size() + 3) / 4); }

void drawPickem(const std::vector<PickemPick> &picks, const std::vector<Game> &games, int seasonW, int seasonL,
                int page) {
  gfxBegin();
  int pages = pickemPages(picks);
  page = ((page % pages) + pages) % pages;
  text3("PICKEM " + String(seasonW) + "-" + String(seasonL), 0, 0, 0xFFD040);
  if (pages > 1) t3R(String(page + 1) + "/" + String(pages), 63, 0, GREY);
  fill(0, 6, 64, 1, 0x303030);
  if (picks.empty()) {
    t3C("NO PICKS", 18, GREY);
    gfxEnd();
    return;
  }
  for (int i = 0; i < 4; i++) {
    unsigned idx = page * 4 + i;
    if (idx >= picks.size()) break;
    const PickemPick &pk = picks[idx];
    int y = 9 + i * 6;
    const Game *g = nullptr;
    for (auto &gm : games)
      if (gm.id == pk.gameId && (pk.league.length() == 0 || gm.league == pk.league)) { g = &gm; break; }
    const Team *me = nullptr, *opp = nullptr;
    if (g) {
      bool pa = g->away.abbr == pk.pick;
      me = pa ? &g->away : &g->home;
      opp = pa ? &g->home : &g->away;
    }
    uint32_t mc = me ? teamColor(*me) : 0xFFFFFF;
    fill(0, y, 1, 5, mc);
    text3(clean3(pk.pick), 3, y, mc);
    text3(">", 14, y, 0x707070);
    text3(opp ? clean3(opp->abbr) : String("---"), 18, y, 0x909090);
    if (pk.result == 'W' || pk.result == 'L' || pk.result == 'T') {
      uint32_t c = pk.result == 'W' ? GREEN : pk.result == 'L' ? RED : GREY;
      fill(57, y - 1, 7, 7, scale(c, 0.9f));
      text3(String(pk.result), 59, y, 0x000000);
      if (g) t3R(String(me->score) + "-" + String(opp->score), 54, y, scale(c, 0.9f));
    } else if (g && g->state == 'i') {
      bool lead = me->score >= opp->score;
      t3R(String(me->score) + "-" + String(opp->score), 59, y, lead ? 0xFFFFFF : 0xA0A0A0);
      int f = 0;
      px(61, y + 2, 0xFF8030), px(62, y + 2, 0xFF8030), px(61, y + 3, 0xFF8030), px(62, y + 3, 0xFF8030);
      (void)f;
    } else if (g && g->state == 'f') {
      t3R(String(me->score) + "-" + String(opp->score), 63, y, 0xA0A0A0);
    } else if (g && g->start) {
      time_t now = time(nullptr);
      String ck = clock12(g->start);
      if (CFG.twelveHour) ck.remove(ck.length() - 1);
      // room for one of them: the time when it's within a day, else the weekday
      t3R(g->start - time(nullptr) < 20 * 3600 ? ck : dowOf(g->start), 63, y, 0x7090D0);
    } else {
      t3R("TBD", 63, y, DIM + 0x202020);
    }
  }
  gfxEnd();
}
