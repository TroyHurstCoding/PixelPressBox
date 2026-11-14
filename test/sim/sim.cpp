// Renders each PixelPressbox screen with the real render.cpp into PPM files: test/sim/out/*.ppm
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "Arduino.h"
#include <cstdlib>
#include "../../src/desk.h"
#include "../../src/render.h"
#include "../../src/holidays.h"
#include "../../src/render2.h"
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

Settings CFG;
extern MatrixPanel_I2S_DMA *simPanel();

static void dump(const char *name) {
  MatrixPanel_I2S_DMA *p = simPanel();
  std::string path = std::string("out/") + name + ".ppm";
  FILE *f = fopen(path.c_str(), "wb");
  fprintf(f, "P6 64 32 255\n");
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 64; x++) {
      uint32_t c = p->shown[y][x];
      unsigned char b[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c};
      fwrite(b, 1, 3, f);
    }
  fclose(f);
}

// portrait screens: write the upright 32x64 view (undo rotation 1: physical x = 63 - y, y = x)
static void dumpTall(const char *name) {
  MatrixPanel_I2S_DMA *p = simPanel();
  std::string path = std::string("out/") + name + ".ppm";
  FILE *f = fopen(path.c_str(), "wb");
  fprintf(f, "P6 32 64 255\n");
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 32; x++) {
      uint32_t c = p->shown[x][63 - y];
      unsigned char b[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c};
      fwrite(b, 1, 3, f);
    }
  fclose(f);
}

int main() {
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();
  displayBegin();
  time_t now = parseIsoUtc("2026-11-12T17:42:00");  // Thu 9:42 AM
  Weather w;
  w.ok = true; w.temp = 58; w.code = 2; w.day = true;
  drawClockFace(now, w, "", 0, false); dump("01_clock");
  drawClockFace(now, w, "MTG 18M", 0xFFC000, false); dump("02_clock_meeting_soon");
  w.code = 61;
  drawClockFace(now, w, "", 0, true); dump("03_clock_busy_rain");

  std::vector<Meeting> ms = {{"Design review with Sam", now + 18 * 60, now + 48 * 60, "busy"},
                             {"Standup", now - 600, now + 900, "busy"}};
  CFG.countdowns = {Countdown{"VEGAS", 2026, 12, 20, 0xFFC000}};
  NowPlaying np; np.playing = true; np.title = "Midnight City"; np.artist = "M83"; np.durationMs = 243000;
  auto cards = buildDeskCards(ms, {"Call Sam back", "MAIL: Q4 budget"}, np, now);
  for (auto &c : cards) {
    if (c.key == "mtg:now") { drawMeeting(c, 2, true); dump("04_meeting_now"); }
    if (c.key == "mtg:next") { drawMeeting(c, 2, false); dump("05_meeting_next"); }
    if (c.kind == CARD_TODO) { drawTodo(c, 0, 0); dump("06_todo"); }
    if (c.kind == CARD_COUNTDOWN) { drawCountdown(c); dump("07_countdown"); }
    if (c.kind == CARD_MUSIC) { drawMusic(np, 101000, 9); dump("08_music"); }
  }
  drawFocus(18 * 60 + 25, 25 * 60, false, now); dump("09_focus");
  drawCelebrateFrame("MEETING", "IN 2M", 0x40A0FF, 0); dump("10_chime");
  drawMessage("ABCDEFGHJ", "see phone", 0xFFC000); dump("11_outlook_code");
  // ---- football
  Game g;
  g.league = "nfl"; g.state = 'i'; g.period = 3; g.clock = "4:12"; g.detail = "4:12 - 3rd";
  g.away.abbr = "LV"; g.away.id = "13"; g.away.color = 0x000000; g.away.alt = 0xA5ACAF; g.away.score = 24;
  g.home.abbr = "KC"; g.home.id = "12"; g.home.color = 0xE31837; g.home.alt = 0xFFB612; g.home.score = 17;
  g.hasSit = true; g.poss = "13"; g.down = "3rd & 7"; g.spot = "KC 35"; g.awayTO = 2; g.homeTO = 3;
  drawFootball(g, now, false); dump("12_football_live");
  drawFootball(g, now, true); dump("13_football_down_distance");
  g.down = "1st & Goal"; g.spot = "KC 4"; g.redzone = true;
  drawFootball(g, now, true); dump("14_football_red_zone");
  g.state = 'f'; g.detail = "Final"; g.hasSit = false;
  drawFootball(g, now, false); dump("15_football_final");
  g.state = 'p'; g.start = now + 3 * 86400; g.away.record = "7-2"; g.home.record = "6-3";
  drawFootball(g, now, false); dump("16_football_pregame");

  // ---- baseball
  Game b;
  b.league = "mlb"; b.state = 'i'; b.period = 7; b.detail = "Bot 7th";
  b.away.abbr = "PIT"; b.away.color = 0x27251F; b.away.alt = 0xFDB827; b.away.score = 11;
  b.home.abbr = "BOS"; b.home.color = 0x0C2340; b.home.alt = 0xBD3039; b.home.score = 5;
  b.hasSit = true; b.on1 = true; b.on3 = true; b.outs = 1; b.balls = 1; b.strikes = 1; b.detail = "Top 7th";
  drawBaseball(b, now); dump("16b_baseball_live");
  b.detail = "Mid 7th";
  drawBaseball(b, now); dump("16c_baseball_mid_inning");
  b.state = 'f'; b.detail = "Final";
  drawBaseball(b, now); dump("16d_baseball_final");
  b.state = 'p'; b.start = now + 2 * 86400; b.away.record = "88-74"; b.home.record = "81-81";
  drawBaseball(b, now); dump("16e_baseball_pregame");

  // ---- holidays + ambient cards
  CFG.holidays = "all";
  auto hcard = [&](const char *date) {
    for (auto &c : buildDeskCards({}, {}, NowPlaying(), parseIsoUtc(date))) if (c.kind == CARD_HOLIDAY) return c;
    return Card();
  };
  drawHoliday(hcard("2026-10-31T19:00:00"), 0); dump("17_holiday_halloween");
  drawHoliday(hcard("2026-12-25T19:00:00"), 0); dump("18_holiday_christmas");
  drawHoliday(hcard("2026-07-04T19:00:00"), 18); dump("19_holiday_july4_fireworks");
  for (auto &c : buildDeskCards({}, {}, NowPlaying(), now))
    if (c.key.startsWith("cdh:")) { drawCountdown(c); dump("20_next_holiday"); }
  CFG.lat = 34.05f; CFG.lon = -118.24f;
  drawSunMoon(now); dump("21_sun_moon");
  drawProgress(now); dump("22_date_progress");
  Weather wd; wd.ok = true; wd.temp = 61; wd.hi = 72; wd.lo = 55; wd.code = 2; wd.feels = 59; wd.uv = 4; wd.rainPct = 20; wd.aqi = 42;
  drawWeatherDetail(wd); dump("23_weather_details");
  Card u;
  u.kind = CARD_UFC; u.text = "UFC 334: Gane vs. Hokit"; u.color = 0xA61212;
  u.when = parseIsoUtc("2026-11-15T01:00:00"); u.until = u.when + 25000;
  drawUfc(u, now, 1); dump("24_ufc_next");
  drawUfc(u, u.when + 3600, 1); dump("25_ufc_live");
  // ---- batch 2
  CFG.favorites = {{"nfl", "LV"}, {"nba", "LAL"}, {"mlb", "LAD"}};
  Card st; st.kind = CARD_STANDINGS; st.title = "AFC WEST";
  st.items = {"1|DEN|8-2|0", "2|KC|6-3|0", "3|LAC|6-4|0", "4|LV|2-7|1"};
  drawStandings(st, false); dump("26_standings_afc_west");
  Card nb; nb.kind = CARD_STANDINGS; nb.title = "WEST";
  nb.items = {"1|LAL|11-2|1", "2|OKC|10-3|0", "3|HOU|9-3|0", "4|MIN|8-4|0"};
  drawStandings(nb, false); dump("27_standings_nba_west");
  auto lines = wrapText(foldAscii("The European Space Agency's lander Philae touched down on 67P/Churyumov\u2013Gerasimenko, becoming the first spacecraft to land on a comet."), 10);
  drawTextCard("TODAY", 0x40A0FF, "2014", 0xFFC000, lines, 0); dump("28_on_this_day");
  lines = wrapText("a series of challenges amounting to an ordeal", 10);
  drawTextCard("gauntlet", 0xFFC000, "n.", 0x707070, lines, 0); dump("29_word_of_the_day");
  lines = wrapText(foldAscii("Why can\u2019t you hear a pterodactyl go to the bathroom? The p is silent."), 10);
  drawTextCard("DAD JOKE", 0xFF8C00, "", 0, lines, 12); dump("30_dad_joke_scrolling");
  drawTetrisClock(now, true);
  for (int i = 0; i < 40; i++) drawTetrisClock(now, false);
  dump("31_tetris_clock_building");
  for (int i = 0; i < 400; i++) drawTetrisClock(now, false);
  dump("32_tetris_clock_done");
  for (int f = 0; f < 60; f++) drawDigitalRain(f);
  dump("33_digital_rain");
  g.state = 'i'; g.hasSit = true; g.redzone = false; g.down = "1st & 10"; g.spot = "NYG 20";
  g.lastPlay = "J.Daniels pass deep left intended for C.Okonkwo INTERCEPTED by J.Holland at NYG 0";
  drawFootball(g, now, false, 10); dump("34_football_last_play");
  drawClockFace(now, w, "IN CALL", 0xFF2020, true); dump("35_clock_teams_in_call");
  // ---- fun screens
  CFG.rainWord = "TROY";
  for (int f = 0; f < 90; f++) drawDigitalRain(f);
  dump("36_rain_hidden_word");
  for (int f = 0; f < 60; f++) drawFireplace(f);
  dump("37_fireplace");
  for (int f = 0; f < 40; f++) drawBouncer(f);
  dump("38_bouncing_logo");
  for (int f = 0; f < 150; f++) drawSnake(f);
  dump("39_snake");
  for (int f = 0; f < 400; f++) drawBreakout(f);
  dump("40_breakout");
  // ---- clock styles (11:30 PM)
  time_t late = parseIsoUtc("2026-11-13T07:30:00");
  CFG.twelveHour = false;
  for (int f = 0; f < 3; f++) drawNixieClock(late, f);
  dump("41_nixie_clock");
  drawBigClock(late); dump("42_big_clock");
  CFG.twelveHour = true;
  drawWordClock(late); dump("43_word_clock");
  drawWordClock(parseIsoUtc("2026-11-13T05:47:00")); dump("44_word_clock_quarter_to");
  drawWordClock(parseIsoUtc("2026-11-12T22:37:00")); dump("47_word_clock_twenty_five");
  for (int f = 0; f < 80; f++) drawFireplace(f);
  dump("45_fireplace_v2");
  CFG.bounceText = "DVD";
  for (int f = 0; f < 60; f++) drawBouncer(f);
  dump("46_bouncer_text");
  // ---- batch 3
  for (int f = 0; f < 120; f++) drawAquarium(f);
  dump("50_aquarium");
  for (int f = 0; f < 70; f++) drawDoublePendulum(f, f == 0);
  dump("51_double_pendulum");
  for (int f = 0; f < 14; f++) drawCradle(f, 0, 1, false, false, f == 0);
  dump("52_newtons_cradle");
  for (int f = 0; f < 60; f++) drawWater(f, 0.55f, 0.85f, true, f == 0);
  dump("53_water_tilted");
  GameResult gr;
  gr.ok = gr.won = true; gr.us = "LAD"; gr.them = "SF"; gr.usScore = 5; gr.themScore = 1;
  for (int f = 0; f < 9; f++) drawFlyW(gr, 0x005A9C, f);
  dump("54_fly_the_w");
  IssFix iss; iss.ok = true; iss.lat = 21.3f; iss.lon = -40.2f; iss.at = millis();
  for (int f = 0; f < 9; f++) drawIssMap(iss, parseIsoUtc("2026-11-13T00:30:00"), 34.05f, -118.24f, f);
  dump("55_iss_day_night");
  drawPlanets(parseIsoUtc("2026-11-13T04:00:00"), 34.05f, -118.24f, 0); dump("56_planets_sky");
  drawPlanets(parseIsoUtc("2026-11-13T04:00:00"), 34.05f, -118.24f, 1); dump("57_planets_list");
  SurfReport sr; sr.ok = true; sr.waveFt = 3.9f; sr.periodS = 11; sr.dirDeg = 254; sr.waterF = 64;
  for (int f = 0; f < 5; f++) drawSurf(sr, "VENICE", f);
  dump("58_surf");
  displaySetRotation(1);
  for (int f = 0; f < 200; f++) drawBeer(f, 0, 1, false, false, f == 0);
  dumpTall("60_beer_pour");
  for (int f = 0; f < 60; f++) drawBeer(200 + f, 0.35f, 0.93f, true, false, false);
  dumpTall("61_beer_tilted");
  drawPortraitClock(parseIsoUtc("2026-11-13T00:47:00")); dumpTall("62_tall_clock");
  for (int f = 0; f < 140; f++) drawHourglass(f, 1, false, f == 0);
  dumpTall("63_hourglass");
  for (int f = 0; f < 150; f++) drawLava(f, f == 0);
  dumpTall("64_lava_lamp");
  displaySetRotation(0);
  puts("rendered");
}
