// render.h - drawing on the 64x32 HUB75 panel.
#pragma once
#include <climits>
#include "playlist.h"
#include "model.h"

bool displayBegin();
void displaySetBrightness(uint8_t b);
void drawMessage(const String &l1, const String &l2 = "", uint32_t color = 0xFFFFFF);
void drawGame(const Game &g, time_t now, bool altPhase);
void drawClock(time_t now, const String &sub, uint32_t subColor);
void drawYahoo(const Card &c);
void drawCelebrateFrame(const String &abbr, const String &text, uint32_t bg, uint32_t fg);
// fantasy scroller: draw one frame with text offset x
void drawScrollFrame(const String &title, uint32_t titleColor, const String &text, int x);

// desk clock screens
// left = "" shows the date; otherwise e.g. "MTG 12M" in leftColor. busy = red frame.
void drawClockFace(time_t now, const Weather &w, const String &left, uint32_t leftColor, bool busy,
                   uint32_t dateColor = 0x707070);  // date tinted on holidays
void drawMeeting(const Card &c, int x, bool busy);       // x = subject scroll offset
void drawTodo(const Card &c, int item, int x);
void drawCountdown(const Card &c);
void drawMusic(const NowPlaying &np, uint32_t progressMs, int x);
void drawFocus(long remain, long total, bool onBreak, time_t now);
int textWidth(const String &s);
void displayTestPattern();
void drawFootball(const Game &g, time_t now, bool altPhase, int playX = INT32_MIN);  // NFL scoreboard; playX scrolls the last play
void drawBaseball(const Game &g, time_t now);                    // TV-style MLB scoreboard
void drawHoliday(const Card &c, int frame);                      // frame animates fireworks
void drawSunMoon(time_t now);
void drawProgress(time_t now);
void drawWeatherDetail(const Weather &w);
void drawUfc(const Card &c, time_t now, int x);                  // x = event name scroll offset
void drawStandings(const Card &c, bool showTitle);               // title flash, then up to 4 rows
void drawTextCard(const String &title, uint32_t titleColor, const String &right, uint32_t rightColor,
                  const std::vector<String> &lines, int offset);  // wrapped text scrolling up under a title
bool drawTetrisClock(time_t now, bool restart);                  // true while blocks are falling
void drawDigitalRain(int frame);                         // CFG.rainWord shows as negative space
void drawFireplace(int frame);
void displayBlit565(const uint16_t *px);                  // show a 64x32 RGB565 picture
void drawBouncer(int frame);
void drawSnake(int frame);
void drawBreakout(int frame);                               // red, green, blue, 0.6 s each                          // pixels at size 1

uint32_t teamColor(const Team &t);
uint32_t teamAlt(const Team &t);
int luminance(uint32_t c);
String statusText(const Game &g, time_t now);
String fmtStart(time_t start, time_t now);
