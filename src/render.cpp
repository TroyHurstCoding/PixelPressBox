#include "render.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "board.h"  // Seengreat RGB Matrix HUB75 S3 pin map
#include "holidays.h"
#include "desk.h"
#include "tronbyt_assets.h"  // nixel tubes + big clock digits (tronbyt/apps, Apache-2.0)
#include <cmath>
#include <cstring>
#ifndef PIXELPRESSBOX_SIM
#include <glcdfont.c>  // the panel font's glyphs, to cut the hidden word out of the rain
#endif

static MatrixPanel_I2S_DMA *dma = nullptr;
#ifdef PIXELPRESSBOX_SIM
MatrixPanel_I2S_DMA *simPanel() { return dma; }
#endif
static const int W = 64, H = 32;
// rows for the 6x8 built-in font
static const int ROW_Y[3] = {2, 12, 23};

static const uint32_t WHITE = 0xFFFFFF, GREY = 0x707070, YELLOW = 0xFFC000, RED = 0xFF2020,
                      GREEN = 0x20FF40, DARK = 0x202020;

static uint16_t c565(uint32_t c) { return dma->color565((c >> 16) & 255, (c >> 8) & 255, c & 255); }

int luminance(uint32_t c) { return (((c >> 16) & 255) * 3 + ((c >> 8) & 255) * 6 + (c & 255)) / 10; }

uint32_t teamColor(const Team &t) {
  uint32_t c = t.color;
  if (luminance(c) < 40) c = t.alt;           // e.g. Raiders black -> silver
  if (luminance(c) < 40) c = 0xA5ACAF;
  return c;
}

uint32_t teamAlt(const Team &t) {
  uint32_t c = t.alt;
  if (luminance(c) < 40 || c == teamColor(t)) c = WHITE;
  return c;
}

static HUB75_I2S_CFG::shift_driver driverFromName(const String &n) {
  if (n == "FM6126A") return HUB75_I2S_CFG::FM6126A;
  if (n == "ICN2038S") return HUB75_I2S_CFG::ICN2038S;
  if (n == "FM6124") return HUB75_I2S_CFG::FM6124;
  if (n == "MBI5124") return HUB75_I2S_CFG::MBI5124;
  return HUB75_I2S_CFG::SHIFTREG;
}

bool displayBegin() {
  HUB75_I2S_CFG::i2s_pins pins = {P_R1, P_G1, P_B1, P_R2, P_G2, P_B2, P_A, P_B, P_C, P_D, P_E, P_LAT, P_OE, P_CLK};
  if (CFG.swapGreenBlue) {  // some panels (Adafruit #5036) have G and B swapped
    std::swap(pins.g1, pins.b1);
    std::swap(pins.g2, pins.b2);
  }
  HUB75_I2S_CFG cfg(W, H, 1, pins);
  cfg.driver = driverFromName(CFG.panelDriver);
  cfg.clkphase = CFG.clkPhase;
  cfg.double_buff = true;  // draw off-screen, then flip -> no flicker
  dma = new MatrixPanel_I2S_DMA(cfg);
  if (!dma->begin()) return false;
  dma->setBrightness8(CFG.brightness);
  dma->setTextWrap(false);
  dma->clearScreen();
  return true;
}

void displaySetBrightness(uint8_t b) {
  if (dma) dma->setBrightness8(b);
}

static void begin() { dma->clearScreen(); dma->fillScreen(0); }
static void end() { dma->flipDMABuffer(); }

// every string goes through foldAscii: the panel font has no curly quotes, dashes or accents
static void text(const String &s, int x, int y, uint32_t color, int size = 1) {
  dma->setTextSize(size);
  dma->setTextColor(c565(color));
  dma->setCursor(x, y);
  dma->print(foldAscii(s));
}

static void textRight(const String &s, int xRight, int y, uint32_t color) {
  String f = foldAscii(s);
  text(f, xRight - (int)f.length() * 6 + 1, y, color);
}

static void textCenter(const String &s, int y, uint32_t color, int size = 1) {
  String f = foldAscii(s);
  int w = f.length() * 6 * size - size;
  text(f, (W - w) / 2, y, color, size);
}

static void rect(int x, int y, int w, int h, uint32_t color) { dma->fillRect(x, y, w, h, c565(color)); }

// ---------------------------------------------------------------- formatting
String fmtStart(time_t start, time_t now) {
  if (!start) return "TBD";
  struct tm s, n;
  localtime_r(&start, &s);
  localtime_r(&now, &n);
  char clock[12];
  int h = s.tm_hour % 12;
  if (h == 0) h = 12;
  snprintf(clock, sizeof(clock), "%d:%02d%c", h, s.tm_min, s.tm_hour >= 12 ? 'P' : 'A');
  if (s.tm_year == n.tm_year && s.tm_yday == n.tm_yday) return clock;
  static const char *DAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  if (start > now && start - now < 6 * 86400) return String(DAYS[s.tm_wday]) + " " + clock;
  return String(s.tm_mon + 1) + "/" + String(s.tm_mday);
}

static String periodName(const Game &g) {
  if (g.league == "nba") {
    if (g.period <= 4) return "Q" + String(g.period);
    return g.period == 5 ? "OT" : String(g.period - 4) + "OT";
  }
  if (g.league == "nfl") return g.period <= 4 ? "Q" + String(g.period) : "OT";
  return "";
}

String statusText(const Game &g, time_t now) {
  if (g.state == 'p') return fmtStart(g.start, now);
  if (g.state == 'f') {
    String d = g.detail;
    d.toUpperCase();
    if (d == "FINAL" || d.length() == 0) return "FINAL";
    d.replace("FINAL/", "F/");
    return d.substring(0, 10);
  }
  if (g.league == "nba" || g.league == "nfl") {
    String d = g.detail;
    d.toLowerCase();
    if (d.indexOf("half") >= 0) return "HALF";
    if (d.indexOf("end") >= 0) return ("END " + periodName(g)).substring(0, 10);
    return (periodName(g) + " " + g.clock).substring(0, 10);
  }
  return g.detail.substring(0, 7);  // MLB: "Top 5th" / "Bot 7th"
}

// ---------------------------------------------------------------- screens
void drawMessage(const String &l1, const String &l2, uint32_t color) {
  begin();
  textCenter(l1.substring(0, 10), 6, color);
  if (l2.length()) textCenter(l2.substring(0, 10), 18, GREY);
  end();
}

void drawGame(const Game &g, time_t now, bool altPhase) {
  begin();
  for (int row = 0; row < 2; row++) {
    const Team &t = row == 0 ? g.away : g.home;
    uint32_t col = teamColor(t);
    int y = ROW_Y[row];
    rect(0, y - 1, 2, 9, col);
    text(t.abbr, 4, y, col);
    if (g.state != 'p') textRight(String(t.score), W - 1, y, WHITE);
    else if (t.record.length()) textRight(t.record, W - 1, y, GREY);
    if (g.hasSit && g.league == "nfl" && g.poss.length() && g.poss == t.id) rect(24, y + 2, 3, 3, YELLOW);
  }
  // status row
  String s = statusText(g, now);
  uint32_t color = g.state == 'i' ? GREEN : (g.state == 'f' ? GREY : YELLOW);
  if (g.hasSit && g.league == "nfl" && g.state == 'i' && altPhase && g.down.length()) {
    s = g.down.substring(0, 10);
    color = g.redzone ? RED : YELLOW;
  }
  text(s, 0, ROW_Y[2], color);
  if (g.hasSit && g.league == "mlb" && g.state == 'i') {
    // base diamond: 2nd top, 3rd left, 1st right
    rect(55, 23, 3, 3, g.on2 ? YELLOW : DARK);
    rect(52, 26, 3, 3, g.on3 ? YELLOW : DARK);
    rect(58, 26, 3, 3, g.on1 ? YELLOW : DARK);
    for (int i = 0; i < 2; i++) rect(45 + i * 3, 29, 2, 2, g.outs > i ? RED : DARK);
  }
  end();
}

void drawClock(time_t now, const String &sub, uint32_t subColor) {
  struct tm t;
  localtime_r(&now, &t);
  char buf[8];
  int h = CFG.twelveHour ? (t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12) : t.tm_hour;
  snprintf(buf, sizeof(buf), "%d:%02d", h, t.tm_min);
  begin();
  textCenter(buf, 2, WHITE, 2);
  String line = sub;
  if (!line.length()) {
    static const char *DAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    line = String(DAYS[t.tm_wday]) + " " + String(t.tm_mon + 1) + "/" + String(t.tm_mday);
    subColor = GREY;
  }
  textCenter(line.substring(0, 10), 22, subColor);
  end();
}

void drawYahoo(const Card &c) {
  bool lead = c.mePts >= c.oppPts;
  begin();
  String me = c.meName.substring(0, 5), opp = c.oppName.substring(0, 5);
  me.toUpperCase();
  opp.toUpperCase();
  text(me, 0, ROW_Y[0], lead ? GREEN : WHITE);
  textRight(String(c.mePts, 1), W - 1, ROW_Y[0], lead ? GREEN : WHITE);
  text(opp, 0, ROW_Y[1], lead ? WHITE : RED);
  textRight(String(c.oppPts, 1), W - 1, ROW_Y[1], lead ? WHITE : RED);
  text("P " + String((int)c.meProj) + "-" + String((int)c.oppProj), 0, ROW_Y[2], GREY);
  end();
}

void drawCelebrateFrame(const String &abbr, const String &txt, uint32_t bg, uint32_t fg) {
  begin();
  rect(0, 0, W, H, bg);
  textCenter(abbr, 4, fg);
  textCenter(txt.substring(0, 10), 19, fg);
  end();
}

void drawScrollFrame(const String &title, uint32_t titleColor, const String &txt, int x) {
  begin();
  text(title.substring(0, 10), 0, ROW_Y[0], titleColor);
  text(txt, x, 16, WHITE);
  end();
}

// ---------------------------------------------------------------- desk clock screens
#include "desk.h"

int textWidth(const String &s) { return foldAscii(s).length() * 6 - 1; }

static void frame(uint32_t color) { dma->drawRect(0, 0, W, H, c565(color)); }

// 7x7 weather icons: y yellow, w cloud, b rain, c snow, m moon
static const char *const ICONS[][7] = {
    {"...y...", ".y...y.", "..yyy..", "y.yyy.y", "..yyy..", ".y...y.", "...y..."},  // clear (day)
    {"..y.y..", "y.yyy..", "..yywww", ".wwwwww", "wwwwwww", "wwwwwww", "......."},  // partly
    {".......", "..www..", ".wwwww.", "wwwwwww", "wwwwwww", ".......", "......."},  // cloud
    {"wwwww..", ".......", ".wwwwww", ".......", "wwwww..", ".......", "..wwwww"},  // fog
    {"..www..", ".wwwww.", "wwwwwww", ".......", "b.b.b..", ".b.b.b.", "b.b.b.."},  // rain
    {"..www..", ".wwwww.", "wwwwwww", ".......", "c..c..c", ".......", ".c..c.."},  // snow
    {"..www..", ".wwwww.", "wwwwwww", "...y...", "..y....", "...y...", "..y...."},  // storm
    {"..mmm..", ".mm....", "mm.....", "mm.....", "mm.....", ".mm....", "..mmm.."},  // clear (night)
};

static void icon(int which, int x, int y) {
  for (int r = 0; r < 7; r++)
    for (int c = 0; c < 7; c++) {
      uint32_t col;
      switch (ICONS[which][r][c]) {
        case 'y': col = YELLOW; break;
        case 'w': col = 0xB0B0B0; break;
        case 'b': col = 0x3080FF; break;
        case 'c': col = 0xE0F0FF; break;
        case 'm': col = 0xF0E090; break;
        default: continue;
      }
      dma->drawPixel(x + c, y + r, c565(col));
    }
}

static String hhmm(time_t now) {
  struct tm t;
  localtime_r(&now, &t);
  int h = CFG.twelveHour ? (t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12) : t.tm_hour;
  char b[8];
  snprintf(b, sizeof(b), "%d:%02d", h, t.tm_min);
  return b;
}

void drawClockFace(time_t now, const Weather &w, const String &left, uint32_t leftColor, bool busy, uint32_t dateColor) {
  struct tm t;
  localtime_r(&now, &t);
  begin();
  if (busy && CFG.busyLight) frame(RED);
  textCenter(hhmm(now), 2, WHITE, 2);
  String l = left;
  if (!l.length()) {
    static const char *DAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    l = String(DAYS[t.tm_wday]) + " " + String(t.tm_mday);
    leftColor = dateColor;
  }
  l = l.substring(0, 7);
  text(l, 2, 22, leftColor);
  if (w.ok) {
    String temp = String(w.temp);
    int tx = 60 - textWidth(temp);  // degree mark sits at x 60-61
    text(temp, tx, 22, WHITE);
    dma->drawRect(60, 22, 2, 2, c565(WHITE));
    WxKind k = wxKind(w.code);
    if (2 + textWidth(l) + 2 <= tx - 9)  // icon only when it doesn't crowd the left text
      icon(k == WX_CLEAR && !w.day ? 7 : (int)k, tx - 9, 22);
  }
  end();
}

void drawMeeting(const Card &c, int x, bool busy) {
  begin();
  text(c.title.substring(0, 10), 2, 2, c.color);
  text(c.text, x, 12, WHITE);
  rect(0, 0, 2, 32, 0);  // keep the scroll inside the frame
  rect(62, 0, 2, 32, 0);
  String span = clockText(c.when) + " " + durationText(c.until - c.when);  // "10:00A 30M"
  text(span.substring(0, 10), 2, 22, GREY);
  if (busy && CFG.busyLight) frame(RED);
  end();
}

void drawTodo(const Card &c, int item, int x) {
  begin();
  text(c.title.substring(0, 7), 0, 2, c.color);
  String pos = String(item + 1) + "/" + String((int)c.items.size());
  textRight(pos, W - 1, 2, GREY);
  if (item < (int)c.items.size()) text(c.items[item], x, 14, WHITE);
  rect(0, 25, W, 1, DARK);
  for (int i = 0; i < (int)c.items.size() && i < 16; i++)  // one dot per item, current one bright
    rect(1 + i * 4, 28, 2, 2, i == item ? c.color : DARK);
  end();
}

void drawCountdown(const Card &c) {
  begin();
  textCenter(c.title.substring(0, 10), 2, c.color);
  if (c.num == 0) {
    textCenter("TODAY", 14, WHITE, 2);
  } else {
    String n = String(c.num), unit = c.num == 1 ? "DAY" : "DAYS";
    int wn = n.length() * 12 - 2, wu = textWidth(unit);
    int x0 = (W - (wn + 3 + wu)) / 2;
    text(n, x0, 14, WHITE, 2);
    text(unit, x0 + wn + 3, 21, GREY);
  }
  end();
}

static void note(int x, int y, uint32_t col) {  // 5x7 music note
  static const char *N[7] = {"..###", "..#.#", "..#.#", "..#..", "###..", "###..", ".#..."};
  for (int r = 0; r < 7; r++)
    for (int c = 0; c < 5; c++)
      if (N[r][c] == '#') dma->drawPixel(x + c, y + r, c565(col));
}

void drawMusic(const NowPlaying &np, uint32_t progressMs, int x) {
  begin();
  text(np.title, x, 2, WHITE);
  rect(0, 0, 8, 11, 0);
  note(0, 2, 0x1ED760);
  text(np.artist.substring(0, 10), 2, 14, GREY);
  rect(2, 27, 60, 2, DARK);
  if (np.durationMs) rect(2, 27, (int)(60ULL * std::min(progressMs, np.durationMs) / np.durationMs), 2, 0x1ED760);
  end();
}

void drawFocus(long remain, long total, bool onBreak, time_t now) {
  uint32_t col = onBreak ? GREEN : 0xFF6020;
  char b[8];
  if (remain < 0) remain = 0;
  snprintf(b, sizeof(b), "%ld:%02ld", remain / 60, remain % 60);
  begin();
  textCenter(b, 2, col, 2);
  rect(2, 19, 60, 2, DARK);
  if (total > 0) rect(2, 19, (int)(60 * (total - remain) / total), 2, col);
  text(onBreak ? "BREAK" : "FOCUS", 2, 23, col);
  textRight(hhmm(now), W - 2, 23, GREY);
  end();
}

void displayTestPattern() {
  const uint32_t cols[] = {0xFF0000, 0x00FF00, 0x0000FF};
  for (uint32_t c : cols) {
    dma->fillScreen(c565(c));
    dma->flipDMABuffer();
    delay(600);
  }
}

// ---------------------------------------------------------------- football
static String compactDown(const String &down, const String &spot) {
  // "3rd & 7" + "LV 35" -> "3&7 LV35";  "1st & Goal" -> "1&G LV4"
  String d = down;
  int amp = d.indexOf('&');
  String out = d;
  if (amp > 0) {
    String n = d.substring(0, 1), togo = d.substring(amp + 1);
    togo.trim();
    if (togo.startsWith("Goal") || togo.startsWith("goal")) togo = "G";
    out = n + "&" + togo;
  }
  String s = spot;
  s.replace(" ", "");
  return s.length() ? out + " " + s : out;
}

static uint32_t textOn(uint32_t bg) { return luminance(bg) > 140 ? 0x000000 : WHITE; }

// team bars + big scores, shared by the football and baseball screens
static void scorebugTop(const Game &g, bool football) {
  for (int side = 0; side < 2; side++) {
    const Team &t = side == 0 ? g.away : g.home;
    const Team &o = side == 0 ? g.home : g.away;
    int x0 = side * 32;
    uint32_t col = teamColor(t);
    rect(x0, 0, 32, 9, col);
    if (side == 0) rect(31, 0, 1, 9, 0);  // hairline between the two bars
    text(t.abbr.substring(0, 3), x0 + 2, 1, textOn(col));
    // possession: small ball in the bar
    if (football && g.state == 'i' && g.hasSit && g.poss.length() && g.poss == t.id) {
      uint32_t bc = textOn(col) == WHITE ? 0xFFD040 : 0x603000;
      rect(x0 + 22, 1, 5, 3, bc);
      rect(x0 + 21, 2, 1, 1, bc);
      rect(x0 + 27, 2, 1, 1, bc);
    }
    // timeouts: three ticks along the bottom of the bar
    int to = side == 0 ? g.awayTO : g.homeTO;
    if (football && g.state == 'i' && to >= 0)
      for (int i = 0; i < 3; i++) rect(x0 + 21 + i * 3, 6, 2, 2, i < to ? textOn(col) : (luminance(col) > 140 ? 0x505050 : 0x404040));
    if (g.state == 'p') {
      if (t.record.length()) {
        int w = textWidth(t.record);
        text(t.record, x0 + (32 - w) / 2, 13, GREY);
      }
    } else {
      String sc = String(t.score);
      bool lost = g.state == 'f' && t.score < o.score;
      uint32_t c = lost ? GREY : WHITE;
      if (sc.length() >= 3) {
        text(sc, x0 + (32 - textWidth(sc)) / 2, 14, c);
      } else {
        int w = sc.length() * 12 - 2;
        text(sc, x0 + (32 - w) / 2, 10, c, 2);
      }
    }
  }
}

void drawFootball(const Game &g, time_t now, bool altPhase, int playX) {
  begin();
  scorebugTop(g, true);
  if (playX != INT32_MIN) {  // last play scrolling across the bottom
    text(g.lastPlay, playX, 25, 0xD0D0D0);
    end();
    return;
  }
  // bottom line: clock, alternating with down & distance while live
  String s = statusText(g, now);
  uint32_t color = g.state == 'i' ? GREEN : (g.state == 'f' ? GREY : YELLOW);
  if (g.state == 'i' && g.hasSit && g.down.length() && altPhase) {
    s = compactDown(g.down, g.spot);
    color = g.redzone ? RED : YELLOW;
  }
  textCenter(s.substring(0, 10), 25, color);
  if (g.state == 'i' && g.redzone) {  // red zone: red corners
    rect(0, 24, 3, 1, RED); rect(0, 24, 1, 8, RED);
    rect(61, 24, 3, 1, RED); rect(63, 24, 1, 8, RED);
  }
  end();
}

// ---------------------------------------------------------------- holidays
static const char *const HICONS[][9] = {
    {"", "", "", "", "", "", "", "", ""},
    {".cc...cc.", "cccc.cccc", "ccccccccc", "ccccccccc", ".ccccccc.", "..ccccc..", "...ccc...", "....c....", "........."},
    {"....n....", "..oonoo..", ".ooooooo.", "oo.o.o.oo", "ooooooooo", "oo.....oo", "ooo.o.ooo", ".ooooooo.", "..ooooo.."},
    {"....y....", "...ggg...", "..ggrgg..", ".ggggggg.", ".grggggg.", "ggggggrgg", "....n....", "....n....", "........."},
    {"...ggg...", "..ggggg..", "gg.ggg.gg", "ggggggggg", "ggggggggg", ".gg.g.gg.", "....g....", "....g....", "...g....."},
    {"bbbbrrrrr", "bwbwwwwww", "bbbbrrrrr", "bwbwwwwww", "rrrrrrrrr", "wwwwwwwww", "rrrrrrrrr", ".........", "........."},
    {"...ppp...", "..ppppp..", ".ppppppp.", ".yyyyyyy.", ".ppppppp.", ".bbbbbbb.", ".ppppppp.", "..ppppp..", "...ppp..."},
    {"......oo.", "....oooo.", "...ooooo.", "..ooooo..", ".oooooo..", ".ooooo...", "..ooo....", ".n.......", "n........"},
    {"....c....", "....c....", "...ccc...", "ccccccccc", ".ccccccc.", "..ccccc..", "..cc.cc..", ".cc...cc.", "........."},
    {"....c....", "....c....", "...ccc...", "ccccccccc", ".ccccccc.", "..ccccc..", "..cc.cc..", ".cc...cc.", "........."},
    {"..wwwww..", ".w.....w.", "w...w...w", "w...w...w", "w...www.w", "w.......w", "w.......w", ".w.....w.", "..wwwww.."},
};

static void holidayIcon(int icon, int x, int y, uint32_t c) {
  for (int r = 0; r < 9; r++) {
    const char *row = HICONS[icon][r];
    for (int k = 0; row[k] && k < 9; k++) {
      uint32_t col;
      switch (row[k]) {
        case 'c': col = c; break;
        case 'r': col = 0xFF2020; break;
        case 'g': col = 0x20C040; break;
        case 'o': col = 0xFF7A00; break;
        case 'y': col = 0xFFD700; break;
        case 'w': col = 0xF0F0F0; break;
        case 'b': col = 0x2050FF; break;
        case 'p': col = 0xFFA0D0; break;
        case 'n': col = 0x8B5A2B; break;
        default: continue;
      }
      dma->drawPixel(x + k, y + r, c565(col));
    }
  }
}

static uint32_t scaleColor(uint32_t c, float f) {
  if (f < 0) f = 0;
  return ((uint32_t)(((c >> 16) & 255) * f) << 16) | ((uint32_t)(((c >> 8) & 255) * f) << 8) | (uint32_t)((c & 255) * f);
}

static void fireworks(int frame) {
  static const uint32_t PAL[] = {0xFF3030, 0xFFD700, 0x30A0FF, 0x40FF60, 0xFF60FF};
  for (int b = 0; b < 3; b++) {
    int f = frame + b * 13, phase = f % 36, cycle = f / 36;
    if (phase >= 28) continue;
    int seed = cycle * 7 + b * 3;
    float cx = 8 + (seed * 37) % 48, cy = 5 + (seed * 17) % 16;
    uint32_t col = scaleColor(PAL[seed % 5], 1.0f - phase / 28.0f);
    float r = phase * 0.55f;
    for (int i = 0; i < 12; i++) {
      float a = i * (float)M_PI / 6;
      int x = (int)lroundf(cx + r * cosf(a)), y = (int)lroundf(cy + r * sinf(a) * 0.8f + phase * phase / 220.0f);
      dma->drawPixel(x, y, c565(col));
    }
  }
}

void drawHoliday(const Card &c, int frame) {
  const Holiday &h = holidayInfo(c.num);
  begin();
  if (h.icon == HI_FIREWORKS) fireworks(frame);
  if (c.title.length()) textCenter(c.title, 2, WHITE);
  textCenter(c.text, c.title.length() ? 12 : 8, c.color);
  if (h.icon != HI_FIREWORKS && h.icon != HI_NONE) holidayIcon(h.icon, 27, 22, c.color);
  end();
}

// ---------------------------------------------------------------- sun & moon
static void moonDisk(int cx, int cy, int r, float p) {
  float k = cosf(2 * (float)M_PI * p);
  for (int y = -r; y <= r; y++) {
    float w = sqrtf((float)(r * r - y * y));
    for (int x = -r; x <= r; x++) {
      if (x * x + y * y > r * r) continue;
      bool lit = p < 0.5f ? x > k * w : x < -k * w;
      dma->drawPixel(cx + x, cy + y, c565(lit ? 0xF0E6B0 : 0x202020));
    }
  }
}

void drawSunMoon(time_t now) {
  begin();
  float p = moonPhase(now);
  moonDisk(8, 9, 7, p);
  time_t rise, set;
  if (sunTimes(CFG.lat, CFG.lon, now, rise, set)) {
    text(String((char)24) + clockText(rise), 20, 2, 0xFFC000);
    text(String((char)25) + clockText(set), 20, 12, 0xFF7A20);
  }
  text(moonName(p), 1, 23, 0xC0C0C0);
  end();
}

// ---------------------------------------------------------------- date progress
void drawProgress(time_t now) {
  float d, m, y;
  dateProgress(now, d, m, y);
  const char *L[] = {"DAY", "MON", "YR"};
  const float V[] = {d, m, y};
  const uint32_t C[] = {0x40A0FF, 0x40E070, 0xFF9030};
  begin();
  for (int i = 0; i < 3; i++) {
    int yy = 2 + i * 10;
    text(L[i], 0, yy, GREY);
    rect(19, yy + 2, 28, 3, DARK);
    rect(19, yy + 2, (int)(28 * V[i] + 0.5f), 3, C[i]);
    String pct = String((int)(V[i] * 100)) + "%";
    textRight(pct, W - 1, yy, C[i]);
  }
  end();
}

// ---------------------------------------------------------------- weather details
static uint32_t uvColor(int uv) {
  return uv <= 2 ? 0x40E070 : uv <= 5 ? 0xFFD700 : uv <= 7 ? 0xFF8020 : uv <= 10 ? 0xFF2020 : 0xC040FF;
}
static uint32_t aqiColor(int a) {
  return a <= 50 ? 0x40E070 : a <= 100 ? 0xFFD700 : a <= 150 ? 0xFF8020 : a <= 200 ? 0xFF2020 : a <= 300 ? 0xC040FF : 0x902030;
}

void drawWeatherDetail(const Weather &w) {
  begin();
  WxKind k = wxKind(w.code);
  icon(k == WX_CLEAR && !w.day ? 7 : (int)k, 0, 2);
  text("H" + String(w.hi) + " L" + String(w.lo), 10, 2, WHITE);
  text("FEEL " + String(w.feels), 0, 12, 0xC0C0C0);
  if (w.rainPct > 0) textRight(String(w.rainPct) + "%", W - 1, 12, 0x40A0FF);
  text("UV " + String(w.uv), 0, 23, uvColor(w.uv));
  if (w.aqi >= 0) textRight("AQI " + String(w.aqi), W - 1, 23, aqiColor(w.aqi));
  end();
}

// ---------------------------------------------------------------- baseball
static void base(int cx, int cy, bool on) {  // 7x7 diamond: outline when empty, filled when occupied
  for (int dy = -3; dy <= 3; dy++) {
    int w = 3 - abs(dy);
    if (on) rect(cx - w, cy + dy, 2 * w + 1, 1, YELLOW);
    else {
      dma->drawPixel(cx - w, cy + dy, c565(WHITE));
      dma->drawPixel(cx + w, cy + dy, c565(WHITE));
    }
  }
}

static void triangle(int x, int y, bool up, uint32_t col) {  // 5 wide, 3 tall
  for (int r = 0; r < 3; r++) {
    int w = up ? r : 2 - r;
    rect(x + 2 - w, y + r, 2 * w + 1, 1, col);
  }
}

void drawBaseball(const Game &g, time_t now) {
  begin();
  // left half: away on top, home below, each on its team color with abbreviation over score
  for (int side = 0; side < 2; side++) {
    const Team &t = side == 0 ? g.away : g.home;
    const Team &o = side == 0 ? g.home : g.away;
    int y = side * 16;
    // dark primary color as the background (near-black teams get a charcoal), bright stripe on the left
    uint32_t bg = luminance(t.color) < 14 ? 0x141414 : t.color;
    if (luminance(bg) > 110) bg = scaleColor(bg, 110.0f / luminance(bg));  // keep white text readable
    rect(0, y, 32, 16, bg);
    rect(0, y, 3, 16, teamColor(t));
    text(t.abbr.substring(0, 4), 6, y + 1, WHITE);
    if (g.state == 'p') {
      text(t.record.substring(0, 5), 6, y + 9, 0xB0B0B0);
    } else {
      bool lost = g.state == 'f' && t.score < o.score;
      text(String(t.score), 6, y + 9, lost ? 0x808080 : WHITE);
    }
  }
  if (g.state != 'i') {  // right half: start time or final
    String st = statusText(g, now);
    int sp = st.indexOf(' ');
    if (g.state == 'p' && sp > 0) {  // "SUN 1:05P" on two lines
      String a = st.substring(0, sp), b = st.substring(sp + 1);
      text(a, 48 - textWidth(a) / 2, 8, YELLOW);
      text(b, 48 - textWidth(b) / 2, 18, YELLOW);
    } else {
      st = st.substring(0, 5);
      text(st, 48 - textWidth(st) / 2, 12, g.state == 'f' ? GREY : YELLOW);
    }
    end();
    return;
  }
  // right half: bases, inning, count, outs
  base(48, 4, g.on2);
  base(42, 10, g.on3);
  base(54, 10, g.on1);
  String d = g.detail;
  d.toLowerCase();
  bool between = d.startsWith("mid") || d.startsWith("end");
  String inn = String(g.period);
  if (between) {
    String lab = (d.startsWith("mid") ? "MID " : "END ") + inn;
    text(lab, 48 - textWidth(lab) / 2, 20, YELLOW);
  } else {
    triangle(34, 25, !d.startsWith("bot"), WHITE);
    text(inn, 41, 23, WHITE);
    String count = String(g.balls) + "-" + String(g.strikes);
    textRight(count, W - 1, 16, WHITE);
    for (int i = 0; i < 2; i++) rect(52 + i * 6, 26, 4, 4, g.outs > i ? RED : 0x404040);
  }
  end();
}

// ---------------------------------------------------------------- UFC
void drawUfc(const Card &c, time_t now, int x) {
  struct tm t;
  localtime_r(&c.when, &t);
  static const char *DAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  static const char *MON[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  begin();
  if (c.when <= now) {
    textCenter("LIVE NOW", 2, 0x20FF40);
    textCenter("FIGHT NITE", 12, WHITE);
  } else {
    textCenter(String(DAYS[t.tm_wday]) + " " + MON[t.tm_mon] + " " + String(t.tm_mday), 2, WHITE);
    long secs = (long)(c.when - now);
    String in = "IN " + (secs < 86400 ? durationText(secs) : String((secs + 43200) / 86400) + "D");
    text(clockText(c.when), 0, 12, YELLOW);
    textRight(in, W - 1, 12, 0x909090);
  }
  rect(0, 22, W, 10, c.color);  // red bar with the event name scrolling
  text(c.text, x, 24, WHITE);
  end();
}

// ---------------------------------------------------------------- standings
void drawStandings(const Card &c, bool showTitle) {
  begin();
  if (showTitle) {
    textCenter(c.title, 12, YELLOW);
    end();
    return;
  }
  for (size_t i = 0; i < c.items.size() && i < 4; i++) {
    // "rank|abbr|record|fav"
    String it = c.items[i];
    int a = it.indexOf('|'), b = it.indexOf('|', a + 1), d = it.indexOf('|', b + 1);
    String rank = it.substring(0, a), abbr = it.substring(a + 1, b), rec = it.substring(b + 1, d);
    bool fav = it.substring(d + 1) == "1";
    int y = (int)i * 8;
    if (fav) rect(0, y, W, 8, 0x302800);
    text(rank, 0, y, fav ? YELLOW : GREY);
    text(abbr, 14, y, fav ? YELLOW : WHITE);
    textRight(rec, W - 1, y, fav ? WHITE : 0xB0B0B0);
  }
  end();
}

// ---------------------------------------------------------------- text cards (history, word, joke)
// title row on top, wrapped lines below scrolling up by `offset` pixels
void drawTextCard(const String &title, uint32_t titleColor, const String &right, uint32_t rightColor,
                  const std::vector<String> &lines, int offset) {
  begin();
  for (size_t i = 0; i < lines.size(); i++) {
    int y = 11 + (int)i * 8 - offset;
    if (y < 3 || y > 31) continue;
    text(lines[i], 2, y, WHITE);
  }
  rect(0, 0, W, 10, 0);  // title bar stays on top of the scrolling text
  text(title, 0, 1, titleColor);
  if (right.length()) textRight(right, W - 1, 1, rightColor);
  rect(0, 9, W, 1, 0x303030);
  end();
}

// ---------------------------------------------------------------- tetris clock
// Each digit is a 3x5 grid of 3-pixel cells, split into pieces of up to 4 connected cells that fall
// in from the top one after another (bottom pieces first), Tetris style.
static const char *const TDIG[10][5] = {
    {"###", "#.#", "#.#", "#.#", "###"}, {".#.", "##.", ".#.", ".#.", "###"}, {"###", "..#", "###", "#..", "###"},
    {"###", "..#", "###", "..#", "###"}, {"#.#", "#.#", "###", "..#", "..#"}, {"###", "#..", "###", "..#", "###"},
    {"###", "#..", "###", "#.#", "###"}, {"###", "..#", "..#", "..#", "..#"}, {"###", "#.#", "###", "#.#", "###"},
    {"###", "#.#", "###", "..#", "###"}};
static const uint32_t TCOL[] = {0x00E0E0, 0xFFD000, 0xB040FF, 0x30E040, 0xFF3030, 0x3070FF, 0xFF8C00};

struct TPiece {
  int8_t cells[4][2];  // (col,row) within the digit
  uint8_t n, color;
};
struct TDigit {
  int value = -1;
  TPiece pieces[15];
  uint8_t count = 0, placed = 0;  // pieces[0..placed) are down, pieces[placed] is falling
  int fall = 0;                   // pixels the falling piece still has to drop
};
static TDigit tdig[4];

static void buildPieces(TDigit &d, int value, int seed) {
  bool used[5][3] = {};
  d.value = value;
  d.count = d.placed = 0;
  // bottom row first, left to right; grow each piece through unused neighbors (up to 4 cells)
  for (int r = 4; r >= 0; r--)
    for (int c = 0; c < 3; c++) {
      if (TDIG[value][r][c] != '#' || used[r][c]) continue;
      TPiece &p = d.pieces[d.count++];
      p.n = 0;
      p.color = (uint8_t)((seed + d.count * 3) % 7);
      int stack[4][2] = {{c, r}};
      int sp = 1;
      used[r][c] = true;
      while (sp > 0 && p.n < 4) {
        sp--;
        int cc = stack[sp][0], rr = stack[sp][1];
        p.cells[p.n][0] = (int8_t)cc;
        p.cells[p.n][1] = (int8_t)rr;
        p.n++;
        const int nb[4][2] = {{cc + 1, rr}, {cc, rr - 1}, {cc - 1, rr}, {cc, rr + 1}};
        for (auto &q : nb) {
          if (q[0] < 0 || q[0] > 2 || q[1] < 0 || q[1] > 4 || used[q[1]][q[0]] || TDIG[value][q[1]][q[0]] != '#') continue;
          if (p.n + sp >= 4) break;
          used[q[1]][q[0]] = true;
          stack[sp][0] = q[0];
          stack[sp][1] = q[1];
          sp++;
        }
      }
    }
  d.fall = 30;
}

// returns true while anything is still falling
bool drawTetrisClock(time_t now, bool restart) {
  struct tm t;
  localtime_r(&now, &t);
  int h = CFG.twelveHour ? (t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12) : t.tm_hour;
  int want[4] = {h / 10, h % 10, t.tm_min / 10, t.tm_min % 10};
  bool animating = false;
  for (int i = 0; i < 4; i++) {
    if (restart || tdig[i].value != want[i]) buildPieces(tdig[i], want[i], i * 2 + want[i]);
    TDigit &d = tdig[i];
    if (d.placed < d.count) {
      animating = true;
      d.fall -= 3;
      if (d.fall <= 0) {
        d.placed++;
        d.fall = 30;
      }
    }
  }
  begin();
  // 4-pixel blocks: digits are 12x20. "9:42" (no leading digit) is centered on its own.
  const int CELL = 4, Y0 = 6;
  bool lead = !(want[0] == 0 && CFG.twelveHour);
  int x0 = lead ? 1 : 9;
  int X[4], colonX;
  if (lead) { X[0] = x0; X[1] = x0 + 14; colonX = x0 + 29; X[2] = x0 + 36; X[3] = x0 + 50; }
  else { X[0] = -100; X[1] = x0; colonX = x0 + 15; X[2] = x0 + 22; X[3] = x0 + 36; }
  for (int i = 0; i < 4; i++) {
    if (i == 0 && !lead) continue;
    TDigit &d = tdig[i];
    for (int p = 0; p < d.count && p <= d.placed; p++) {
      if (p == d.placed && d.placed >= d.count) break;
      int drop = p == d.placed ? d.fall : 0;
      const TPiece &pc = d.pieces[p];
      for (int k = 0; k < pc.n; k++) {
        int x = X[i] + pc.cells[k][0] * CELL, y = Y0 + pc.cells[k][1] * CELL - drop;
        if (y + CELL <= 0) continue;
        uint32_t c = TCOL[pc.color], dark = scaleColor(c, 0.5f);
        rect(x, y, CELL, CELL, c);
        rect(x + CELL - 1, y, 1, CELL, dark);  // shade the right and bottom edge of each block
        rect(x, y + CELL - 1, CELL, 1, dark);
      }
    }
  }
  if (t.tm_sec % 2 == 0 || animating) {  // blinking colon
    rect(colonX, 11, 2, 2, WHITE);
    rect(colonX, 19, 2, 2, WHITE);
  }
  end();
  return animating;
}

// ---------------------------------------------------------------- digital rain
static int8_t rainY[64];
static uint8_t rainSpeed[64], rainLen[64];
static uint32_t rainSeed = 12345;
static uint32_t rnd() { return rainSeed = rainSeed * 1103515245u + 12345u; }

// mask of the hidden word: true where a letter is (rain skips those pixels, so the word shows as black gaps)
static bool rainMask[32][64];
static String rainMaskWord = "\x01";
static void buildRainMask(const String &w) {
  memset(rainMask, 0, sizeof(rainMask));
  rainMaskWord = w;
  String s = foldAscii(w);
  if (!s.length()) return;
  int sc = s.length() <= 5 ? 2 : 1;            // big letters for short words
  if (s.length() > 10) s = s.substring(0, 10);
  int wpx = s.length() * 6 * sc - sc, x0 = (64 - wpx) / 2, y0 = (32 - 7 * sc) / 2;
  for (unsigned i = 0; i < s.length(); i++) {
    unsigned char ch = s[i];
    for (int col = 0; col < 5; col++) {
      unsigned char bits = font[ch * 5 + col];
      for (int row = 0; row < 8; row++, bits >>= 1)
        if (bits & 1)
          for (int dy = 0; dy < sc; dy++)
            for (int dx = 0; dx < sc; dx++) {
              int x = x0 + (i * 6 + col) * sc + dx, y = y0 + row * sc + dy;
              if (x >= 0 && x < 64 && y >= 0 && y < 32) rainMask[y][x] = true;
            }
    }
  }
}

void drawDigitalRain(int frame) {
  if (CFG.rainWord != rainMaskWord) buildRainMask(CFG.rainWord);
  bool word = CFG.rainWord.length() > 0;
  // with a hidden word the rain is denser (longer trails, shorter gaps) so the letters read clearly
  const int lenMin = word ? 18 : 6, lenRange = word ? 14 : 12, gap = word ? 4 : 16;
  if (frame == 0)
    for (int x = 0; x < 64; x++) {
      rainY[x] = -(int8_t)(rnd() % 32);
      rainSpeed[x] = 1 + rnd() % 3;
      rainLen[x] = lenMin + rnd() % lenRange;
    }
  begin();
  for (int x = 0; x < 64; x++) {
    if (frame % rainSpeed[x] == 0) rainY[x]++;
    if (rainY[x] - rainLen[x] > 32) {
      rainY[x] = -(int8_t)(rnd() % gap);
      rainSpeed[x] = 1 + rnd() % 3;
      rainLen[x] = lenMin + rnd() % lenRange;
    }
    for (int k = 0; k < rainLen[x]; k++) {
      int y = rainY[x] - k;
      if (y < 0 || y > 31) continue;
      if (word && rainMask[y][x]) continue;  // normal rain on black, but none falls where the letters are
      if (k == 0) dma->drawPixel(x, y, c565(0xD0FFD0));  // bright head
      else {  // with a word the trails fade less, so the black letters stand out against them
        float b = 1.0f - (float)k / rainLen[x];
        if (word) b = 0.35f + 0.65f * b;
        dma->drawPixel(x, y, c565(scaleColor(0x00FF40, b)));
      }
    }
  }
  end();
}

// ---------------------------------------------------------------- fireplace
// classic "fire" cellular automaton: heat rises from embers on the logs, cools and drifts. Dark, warm palette.
static uint8_t heat[36][64];
static uint32_t firePal(int v) {  // 0..255 -> black, deep red, orange, yellow, pale yellow
  if (v < 40) return 0;
  if (v < 100) return ((uint32_t)((v - 40) * 2) << 16);                                  // up to (120,0,0)
  if (v < 170) return ((uint32_t)(120 + (v - 100) * 2) << 16) | ((uint32_t)((v - 100)) << 8);  // to (260~,70,0)
  if (v < 225) return (0xFF0000) | ((uint32_t)(70 + (v - 170) * 2) << 8);                // to (255,180,0)
  return 0xFF0000 | ((uint32_t)(180 + (v - 225)) << 8) | (uint32_t)((v - 225) * 3);     // to pale yellow
}
void drawFireplace(int frame) {
  if (frame == 0) memset(heat, 0, sizeof(heat));
  // embers: hottest over the middle of the logs, a few flaring spots move around
  for (int x = 0; x < 64; x++) {
    int d = abs(x - 32);
    int base = d > 19 ? 0 : 255 - d * 4;
    heat[35][x] = base ? (uint8_t)std::max(0, base - (int)(rnd() % 90)) : 0;
  }
  for (int i = 0; i < 3; i++) heat[35][14 + rnd() % 36] = 255;
  for (int y = 0; y < 35; y++)
    for (int x = 0; x < 64; x++) {
      int drift = (int)(rnd() % 3) - 1;
      int sx = std::min(63, std::max(0, x + drift));
      int v = (heat[y + 1][sx] * 2 + heat[y + 1][std::max(0, x - 1)] + heat[y + 1][std::min(63, x + 1)]) / 4;
      v -= 3 + rnd() % 8;  // cooling: tongues of flame with dark gaps
      heat[y][x] = v < 0 ? 0 : v;
    }
  begin();
  for (int y = 0; y < 27; y++)
    for (int x = 0; x < 64; x++) {
      uint32_t c = firePal(heat[y + 9][x]);
      if (c) dma->drawPixel(x, y, c565(c));
    }
  // logs with a glowing underside
  rect(8, 26, 48, 3, 0x3A2010);
  rect(14, 29, 38, 3, 0x2E180C);
  for (int x = 9; x < 55; x += 3) dma->drawPixel(x, 26, c565(0xC04010 + ((rnd() % 3) << 12)));
  rect(8, 26, 2, 3, 0x6A4020);
  rect(50, 29, 2, 3, 0x6A4020);
  end();
}

// ---------------------------------------------------------------- bouncing logo
static float bx = 3, by = 4, bvx = 0.7f, bvy = 0.45f;
static int bcol = 0;
void drawBouncer(int frame) {
  static const uint32_t COLS[] = {0xFF3030, 0x30A0FF, 0x40FF60, 0xFFD000, 0xFF40FF, 0x40FFFF, 0xFF8020};
  String t = foldAscii(CFG.bounceText).substring(0, 9);
  int w = t.length() * 6 + 3, h = 11;
  bx += bvx;
  by += bvy;
  bool hitX = false, hitY = false;
  if (bx <= 0 || bx + w >= 64) { bvx = -bvx; bx = bx <= 0 ? 0 : 64 - w; hitX = true; }
  if (by <= 0 || by + h >= 32) { bvy = -bvy; by = by <= 0 ? 0 : 32 - h; hitY = true; }
  if (hitX || hitY) bcol = (bcol + 1) % 7;
  static int cornerFlash = 0;
  if (hitX && hitY) cornerFlash = 20;  // the famous corner hit
  begin();
  uint32_t c = cornerFlash > 0 && (cornerFlash-- / 3) % 2 ? WHITE : COLS[bcol];
  text(t, (int)bx + 1, (int)by + 2, c);  // just the colored letters bouncing around
  (void)frame;
  end();
}

// ---------------------------------------------------------------- snake (plays itself)
static const int SW = 32, SH = 16;  // 2x2-pixel cells
static int8_t snX[SW * SH], snY[SW * SH];
static int snLen = 0, snDir = 0, foodX = 0, foodY = 0;
static bool snBody(int x, int y, int skipTail) {
  for (int i = 0; i < snLen - skipTail; i++)
    if (snX[i] == x && snY[i] == y) return true;
  return false;
}
static void snFood() {
  do {
    foodX = rnd() % SW;
    foodY = rnd() % SH;
  } while (snBody(foodX, foodY, 0));
}
static void snReset() {
  snLen = 4;
  for (int i = 0; i < snLen; i++) { snX[i] = 10 - i; snY[i] = 8; }
  snDir = 0;
  snFood();
}
void drawSnake(int frame) {
  if (frame == 0 && snLen == 0) snReset();
  static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};
  // greedy AI: of the moves that don't crash, take the one closest to the food (ties keep going straight)
  int best = -1, bestD = 1 << 30;
  for (int k = 0; k < 4; k++) {
    int d = (snDir + k) % 4;
    if (k == 2) continue;  // no reversing
    int nx = snX[0] + DX[d], ny = snY[0] + DY[d];
    if (nx < 0 || nx >= SW || ny < 0 || ny >= SH || snBody(nx, ny, 1)) continue;
    // look one step further so it doesn't walk into a dead end
    int exits = 0;
    for (int e = 0; e < 4; e++) {
      int ex = nx + DX[e], ey = ny + DY[e];
      if (ex >= 0 && ex < SW && ey >= 0 && ey < SH && !snBody(ex, ey, 1) && !(ex == snX[0] && ey == snY[0])) exits++;
    }
    int dist = abs(nx - foodX) + abs(ny - foodY) + (exits == 0 ? 1000 : 0) + (k == 0 ? 0 : 1);
    if (dist < bestD) { bestD = dist; best = d; }
  }
  if (best < 0) {
    snReset();
  } else {
    snDir = best;
    int nx = snX[0] + DX[best], ny = snY[0] + DY[best];
    bool eat = nx == foodX && ny == foodY;
    if (eat && snLen < SW * SH - 1) snLen++;
    for (int i = snLen - 1; i > 0; i--) { snX[i] = snX[i - 1]; snY[i] = snY[i - 1]; }
    snX[0] = nx;
    snY[0] = ny;
    if (eat) snFood();
    if (snLen > 120) snReset();  // fill-up limit: start over
  }
  begin();
  rect(foodX * 2, foodY * 2, 2, 2, 0xFF3030);
  for (int i = snLen - 1; i >= 0; i--)
    rect(snX[i] * 2, snY[i] * 2, 2, 2, i == 0 ? 0xC0FF80 : scaleColor(0x30E040, 1.0f - 0.5f * i / snLen));
  end();
}

// ---------------------------------------------------------------- breakout (plays itself)
static bool bricks[5][16];
static float ballX, ballY, ballVX, ballVY, padX;
static int bricksLeft = 0;
static void boReset() {
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < 16; c++) bricks[r][c] = true;
  bricksLeft = 80;
  ballX = 32; ballY = 22; ballVX = 0.55f; ballVY = -0.6f; padX = 26;
}
void drawBreakout(int frame) {
  if (frame == 0 && bricksLeft == 0) boReset();
  static const uint32_t ROWC[5] = {0xFF3030, 0xFF8C00, 0xFFD000, 0x30E040, 0x3080FF};
  for (int step = 0; step < 2; step++) {
    ballX += ballVX;
    ballY += ballVY;
    if (ballX < 0) { ballX = 0; ballVX = -ballVX; }
    if (ballX > 63) { ballX = 63; ballVX = -ballVX; }
    if (ballY < 0) { ballY = 0; ballVY = -ballVY; }
    int r = ((int)ballY - 2) / 3, c = (int)ballX / 4;  // bricks: rows of 3 px from y=2, columns of 4 px
    if (ballY >= 2 && r >= 0 && r < 5 && c >= 0 && c < 16 && bricks[r][c]) {
      bricks[r][c] = false;
      bricksLeft--;
      ballVY = -ballVY;
    }
    if (ballY >= 29 && ballY < 30.5f && ballVY > 0 && ballX >= padX - 1 && ballX <= padX + 11) {
      ballVY = -ballVY;
      ballVX = (ballX - (padX + 5)) * 0.12f;  // where it hits the paddle sets the angle
      if (fabsf(ballVX) < 0.25f) ballVX = ballVX < 0 ? -0.25f : 0.25f;
    }
    if (ballY > 32) boReset();
  }
  // paddle AI follows the ball (a little lazily)
  float target = ballX - 5;
  padX += (target - padX) * 0.25f;
  if (padX < 0) padX = 0;
  if (padX > 54) padX = 54;
  if (bricksLeft == 0) boReset();
  begin();
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < 16; c++)
      if (bricks[r][c]) rect(c * 4, 2 + r * 3, 3, 2, ROWC[r]);
  rect((int)padX, 30, 10, 2, 0xC0C0C0);
  rect((int)ballX, (int)ballY, 1, 1, WHITE);
  (void)frame;
  end();
}

void displayBlit565(const uint16_t *px) {
  begin();
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      if (px[y * W + x]) dma->drawPixel(x, y, px[y * W + x]);
  end();
}

// ---------------------------------------------------------------- nixel clock (after "Nixel Clock" by saltedlolly)
// Photographed nixie tubes, 15x15 each, 4 tubes + a 4px separator = 64 px. Each tube breathes on its own slow
// wave (dims up to 20/255 like the original). Artwork: tronbyt/apps, Apache-2.0 (src/tronbyt_assets.h, NOTICE).
static int clockDigits(time_t now, int d[4]) {
  struct tm t;
  localtime_r(&now, &t);
  int h = CFG.twelveHour ? (t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12) : t.tm_hour;
  d[0] = h / 10; d[1] = h % 10; d[2] = t.tm_min / 10; d[3] = t.tm_min % 10;
  return t.tm_sec;
}

static uint16_t dim565(uint16_t c, int amount) {  // black overlay with alpha `amount` (0..255)
  if (!amount) return c;
  int k = 255 - amount;
  int r = ((c >> 11) & 0x1F) * k / 255, g = ((c >> 5) & 0x3F) * k / 255, b = (c & 0x1F) * k / 255;
  return (uint16_t)((r << 11) | (g << 5) | b);
}
static void blit565(const uint16_t *px, int x0, int y0, int w, int h, int amount) {
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      uint16_t c = px[y * w + x];
      if (c) dma->drawPixel(x0 + x, y0 + y, dim565(c, amount));
    }
}

void drawNixieClock(time_t now, int frame) {
  static const float WAVES[5][2][2] = {{{1, 0.00f}, {2, 0.40f}}, {{1, 0.55f}, {2, 0.10f}}, {{1, 0.30f}, {2, 0.75f}},
                                       {{1, 0.85f}, {2, 0.20f}}, {{1, 0.65f}, {2, 0.50f}}};
  int d[4];
  clockDigits(now, d);
#ifdef PIXELPRESSBOX_SIM
  int f = frame % 16;
#else
  int f = (millis() / 250) % 16;
#endif  // the original's 16-frame, 250 ms glow loop
  int dims[5];
  for (int i = 0; i < 5; i++) {
    float lv = 0;
    for (int k = 0; k < 2; k++) lv += sinf(2 * (float)M_PI * (WAVES[i][k][0] * f / 16.0f + WAVES[i][k][1]));
    lv = (lv / 2 + 1) / 2;
    dims[i] = (int)(lv * 20 / 4 + 0.5f) * 4;
  }
  begin();
  const int X[5] = {0, 15, 30, 34, 49}, Y = 8;  // row is centred: (32 - 15) / 2
  for (int i = 0; i < 4; i++) {
    if (i == 0 && d[0] == 0 && CFG.twelveHour) continue;  // no leading zero in 12-hour time
    int xi = i < 2 ? X[i] : X[i + 1];
    blit565(NIXEL_DIGITS + d[i] * 225, xi, Y, 15, 15, dims[i]);
  }
  blit565(NIXEL_SEP, X[2], Y, 4, 15, dims[4]);
  end();
}

// ---------------------------------------------------------------- big clock (after "Big Clock" by Joey Hoer)
// Full-height 13x32 beveled segment digits spread edge to edge, blinking colon. Masks from tronbyt/apps.
static void bigMask(int x0, int dgt, uint32_t col) {
  uint16_t c = c565(col);
  for (int y = 0; y < 32; y++) {
    uint16_t bits = BIG_DIGITS[dgt * 32 + y];
    for (int x = 0; x < 13; x++)
      if (bits & (1 << (12 - x))) dma->drawPixel(x0 + x, y, c);
  }
}

void drawBigClock(time_t now) {
  int d[4];
  int sec = clockDigits(now, d);
  begin();
  // Row(space_between): 13 + 13 + 4 + 13 + 13 = 56, the 8 spare pixels split into four 2 px gaps
  const int X[4] = {0, 15, 36, 51};
  for (int i = 0; i < 4; i++)
    if (!(i == 0 && d[0] == 0 && CFG.twelveHour)) bigMask(X[i], d[i], WHITE);
  if (sec % 2 == 0)
    for (int y = 0; y < 14; y++) {
      uint8_t bits = BIG_SEP[y];
      for (int x = 0; x < 4; x++)
        if (bits & (8 >> x)) dma->drawPixel(30 + x, 9 + y, c565(WHITE));
    }
  end();
}

// ---------------------------------------------------------------- word clock (after "Fuzzy Clock" by Max Timkovich)
// Tidbyt's 5x8 font look: 4x6 capitals on a 5 px pitch, three lines stepping right one character each.
static const char *const F5[] = {  // A-Z, then '-' and '\''; 6 rows of 4 pixels, '#' = lit
    ".##.#..##..######..##..#", "###.#..####.#..##..####.", ".##.#..##...#...#..#.##.", "###.#..##..##..##..####.",
    "#####...###.#...#...####", "#####...###.#...#...#...", ".##.#..##...#.###..#.###", "#..##..######..##..##..#",
    "###..#...#...#...#..###.", "..##...#...#...##..#.##.", "#..##.#.##..##..#.#.#..#", "#...#...#...#...#...####",
    "#..##########..##..##..#", "#..###.###.##.###.###..#", ".##.#..##..##..##..#.##.", "###.#..##..####.#...#...",
    ".##.#..##..##..###.#.##.", "###.#..##..####.#.#.#..#", ".##.#..#.#....#.#..#.##.", "###..#...#...#...#...#..",
    "#..##..##..##..##..#.##.", "#..##..##..##..#.##..##.", "#..##..##..##########..#", "#..##..#.##..##.#..##..#",
    "#.#.#.#.#.#..#...#...#..", "####...#..#..#..#...####", "........###.............", ".#...#..................",
};
static void fuzzyChar(char ch, int x, int y, uint32_t col) {
  const char *g = nullptr;
  if (ch >= 'A' && ch <= 'Z') g = F5[ch - 'A'];
  else if (ch == '-') g = F5[26];
  else if (ch == '\'') g = F5[27];
  if (!g) return;
  uint16_t c = c565(col);
  for (int i = 0; i < 24 && g[i]; i++)
    if (g[i] == '#') dma->drawPixel(x + i % 4, y + i / 4, c);
  if (ch == 'Q') dma->drawPixel(x + 3, y + 6, c);  // Q's tail
}

void drawWordClock(time_t now) {
  struct tm t;
  localtime_r(&now, &t);
  std::vector<String> lines = wordClockLines(t.tm_hour, t.tm_min);
  int n = lines.size();
  // padding 4 top/bottom, lines 8 tall, spaced evenly in the 24 px between (as the original's Column)
  float gap = (24.0f - n * 8) / (n + 1);
  begin();
  for (int i = 0; i < n; i++) {
    int y = 4 + (int)(gap * (i + 1) + i * 8 + 0.5f) + 1;
    int x = 3 + i * 5;
    for (unsigned k = 0; k < lines[i].length(); k++) fuzzyChar(lines[i][k], x + k * 5, y, WHITE);
  }
  end();
}
