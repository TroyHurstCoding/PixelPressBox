// render5.h - batch 5 screens.
//   render5a.cpp: clocks (split-flap, analog with dials, terminal spinner, slot machine) and life in weeks
//   render5b.cpp: toys (robot eyes, magic 8 ball, party tools, sand timer) and the maze
//   render5d.cpp: data screens (launches, playoff race, park waits, stocks, news, last play, drive time,
//                 Powerball, pick'em)
// All draw functions draw one whole frame (gfxBegin ... gfxEnd) and keep any animation state in statics.
// "frame" counts up from 0 each time the screen comes up (restart when frame == 0).
// Times are UTC epoch seconds; use localtime_r for wall-clock values (TZ is set by the firmware).
#pragma once
#include "model.h"

// ---------------------------------------------------------------- render5a: clocks
// split-flap board: HH:MM digits flip (top half falls over the bottom) when they change. Second row: `sub`
// (e.g. "BUF VS MIA 1:00 CBS"), scrolled when too long; "" = date instead. Returns true on the frame a flip starts
// (the caller plays a click).
bool drawFlapClock(time_t now, const String &sub, uint32_t subColor, int frame);
// analog face (left, 32x32) plus three small dials on the right: temperature (needle over a lo..hi arc, or
// "--" with no weather), date (day of month in a ring, weekday letters), and a ring that fills as the next game
// gets closer (full = game time; gameStart 0 = empty ring with "NO GM").
void drawDialsClock(time_t now, const Weather &w, time_t gameStart, const String &gameAbbr, uint32_t gameColor);
// green-on-black terminal: "$ date" prompt, the time typed out, a | / - \ spinner ticking each second, blinking
// cursor; every minute or so a fake command ("$ uptime", "$ whoami") types out and answers.
void drawTerminalClock(time_t now, int frame);
// slot machine: four reels show HH MM; each minute the reels spin and stop left to right; on the hour a jackpot
// flash with coins. frame at 30 fps.
void drawSlotClock(time_t now, int frame);
// life in weeks: grid of lifeYears*52 weeks (each week one pixel, years as columns), lived weeks lit, this week
// blinking; a caption with weeks lived and percent.
void drawLifeWeeks(time_t now, int birthYear, int birthMonth, int birthDay, int lifeYears, int frame);

// ---------------------------------------------------------------- render5b: toys
// robot eyes: two big rounded eyes, pupils follow (gx, gy) gravity in g (screen terms); blink every few seconds;
// mood: 0 normal, 1 excited (wide, sparkles; e.g. a favorite scored), 2 sleepy (half closed, at night).
void drawEyes(int frame, float gx, float gy, int mood);
// magic 8 ball: idle = an "8" ball with "SHAKE ME"/"ASK"; shaken = true on the frame the clock was shaken (or
// tapped), the triangle floats up out of dark blue liquid with a random answer (classic 20 answers) and stays.
void drawEightBall(int frame, bool shaken);
// party tools: tool 0 two dice, 1 coin flip, 2 spinner wheel (numbers 1-8 in colours); shaken = roll/flip/spin now.
// tap cycles the tool (caller passes tool). Rolls animate ~1 s then settle.
void drawParty(int tool, int frame, bool shaken);
// sand timer (landscape): an hourglass on the left draining as the time runs out, the time left (M:SS) on the
// right. totalSec 0 = setting mode: shows "+15S TAP" and the time chosen so far (setSec). remainSec <= 0 and
// totalSec > 0 = done: flashing "TIME!".
void drawSandTimer(int totalSec, int remainSec, int setSec, int frame);
// maze: generate a random maze (recursive backtracker, 2-px cells on a 64x32 board), draw the carving, then solve
// it with a glowing path, hold, start again. frame at 30 fps; restart when frame == 0.
void drawMaze(int frame);

// ---------------------------------------------------------------- render5d: data screens
void drawLaunch(const Launch &l, time_t now, int frame);              // T-minus countdown; "LIFTOFF" after net
// playoff race for one team: seed / games back / magic number from a standings table (see playoffLine())
void drawPlayoff(const StandGroup &g, const String &team, uint32_t color, int frame);
// longest waits first, 4 per page; waits >= 60 min in red, 30-59 amber, else green
int parkPages(const ParkWaits &p);
void drawParkWaits(const ParkWaits &p, int page, int frame);
// one stock: symbol, price, change vs prevClose (or vs open when prevClose is 0), day range bar, sparkline
void drawStock(const StockQuote &q, int frame);
// news: banner = the headline sliding down from the top over whatever is on screen (y offset animates);
// list = the recent alerts card, `sel` = which item is scrolling
void drawNewsBanner(const NewsItem &n, int frame, int scrollX);
void drawNewsList(const std::vector<NewsItem> &items, int sel, int scrollX);
// last play of a live favorite game: score bug on top, down & distance, the play scrolling at the bottom
void drawLastPlay(const Game &g, int scrollX);
void drawDriveTime(const DriveTime &d, time_t now, int frame);
void drawLotto(const LottoDraw &d, time_t now, int frame);
// pick'em: each pick with its game state (green W / red L / grey pending with score), record at the top
int pickemPages(const std::vector<PickemPick> &picks);
void drawPickem(const std::vector<PickemPick> &picks, const std::vector<Game> &games, int seasonW, int seasonL,
                int page);
