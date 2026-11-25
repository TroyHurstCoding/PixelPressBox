// sim for render5a: clocks + life in weeks -> out/A*.ppm
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "Arduino.h"
#include <cstdlib>
#include <ctime>
#include "../../src/render.h"
#include "../../src/render5.h"
#include "simdump.h"

Settings CFG;

static time_t mk(int y, int mo, int d, int h, int mi, int s) {
  struct tm t = {};
  t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d; t.tm_hour = h; t.tm_min = mi; t.tm_sec = s; t.tm_isdst = -1;
  return mktime(&t);
}

int main() {
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();
  displayBegin();
  Weather w; w.ok = true; w.temp = 58; w.lo = 44; w.hi = 66; w.code = 2;
  // ---- flap
  time_t t0 = mk(2026, 11, 12, 9, 41, 58);
  for (int f = 0; f < 40; f++) { drawFlapClock(t0 + (f >= 10 ? 2 : 0), "", 0, f); }  // 09:42:00 at f=10
  drawFlapClock(t0, "", 0, 0); dump("A1_flap_date");
  drawFlapClock(t0 + 2, "", 0, 1); drawFlapClock(t0 + 2, "", 0, 4); dump("A1_flap_midflip");
  drawFlapClock(t0 + 2, "", 0, 30); dump("A1_flap_after");
  drawFlapClock(t0 + 2, "BUF VS MIA 1:00 CBS", 0x40FF60, 50); dump("A1_flap_sub_scroll");
  drawFlapClock(t0 + 2, "NO GAME", 0xFF8040, 60); dump("A1_flap_sub_short");
  // hour change: 12:59 -> 1:00
  drawFlapClock(mk(2026, 11, 12, 12, 59, 59), "", 0, 0);
  drawFlapClock(mk(2026, 11, 12, 13, 0, 0), "", 0, 1);
  drawFlapClock(mk(2026, 11, 12, 13, 0, 0), "", 0, 5); dump("A1_flap_allflip");
  // ---- dials
  time_t n = mk(2026, 11, 12, 10, 8, 37);
  drawDialsClock(n, w, n + 3 * 3600 + 900, "BUF", 0x40A0FF); dump("A2_dials_game");
  drawDialsClock(mk(2026, 11, 12, 3, 45, 5), w, 0, "", 0); dump("A2_dials_nogame");
  Weather nw;
  drawDialsClock(mk(2026, 11, 28, 6, 20, 50), nw, mk(2026, 11, 28, 6, 15, 0), "KC", 0xFF4040); dump("A2_dials_noweather_live");
  w.temp = 66; w.hi = 66;
  drawDialsClock(mk(2026, 11, 3, 12, 0, 0), w, mk(2026, 11, 3, 12, 40, 0), "DAL", 0x40FF60); dump("A2_dials_hot");
  // ---- terminal
  time_t tt = mk(2026, 11, 12, 21, 7, 9);
  drawTerminalClock(tt, 0);
  drawTerminalClock(tt, 5); dump("A3_term_typing");
  drawTerminalClock(tt, 20); dump("A3_term_date");
  drawTerminalClock(tt, 60); dump("A3_term_full");
  time_t tf = mk(2026, 11, 12, 21, 7, 21);
  for (int f = 61; f <= 90; f++) drawTerminalClock(tf, f);
  dump("A3_term_fake_cmd");
  for (int f = 91; f <= 130; f++) drawTerminalClock(tf, f);
  dump("A3_term_fake_out");
  // ---- slot
  time_t sl = mk(2026, 11, 12, 9, 42, 15);
  drawSlotClock(sl, 0); drawSlotClock(sl, 12); dump("A4_slot_spin");
  drawSlotClock(sl, 40); dump("A4_slot_partial");
  drawSlotClock(sl, 100); dump("A4_slot_stopped");
  time_t sj = mk(2026, 11, 12, 10, 0, 0);
  drawSlotClock(sj - 60, 200);
  drawSlotClock(sj, 201); drawSlotClock(sj, 215); dump("A4_slot_spin2");
  drawSlotClock(sj, 201 + 66 + 8); dump("A4_slot_jackpot1");
  drawSlotClock(sj, 201 + 66 + 20); dump("A4_slot_jackpot2");
  // ---- life
  time_t ln = mk(2026, 11, 12, 12, 0, 0);
  drawLifeWeeks(ln, 1979, 3, 15, 80, 0); dump("A5_life_a");
  drawLifeWeeks(ln, 1979, 3, 15, 80, 14); dump("A5_life_b");
  drawLifeWeeks(ln, 1995, 6, 1, 90, 0); dump("A5_life_90");
  drawLifeWeeks(ln, 2015, 6, 1, 80, 0); dump("A5_life_kid");
  drawLifeWeeks(ln, 0, 0, 0, 80, 0); dump("A5_life_unset");
  CFG.twelveHour = false;
  drawFlapClock(t0 + 2, "", 0, 100); dump("A6_flap_24h");
  drawTerminalClock(tt, 100); dump("A6_term_24h");
  drawSlotClock(mk(2026, 11, 12, 21, 42, 15), 300); dump("A6_slot_24h");
  return 0;
}
