// Desktop tests for the C++ ticker logic. Network calls are replaced by JSON fixtures,
// and the real ArduinoJson filter path is exercised.
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "Arduino.h"
#include <ArduinoJson.h>
#include <cassert>
#include <fstream>
#include <sstream>
#include "../../src/espn.h"
#include "../../src/net.h"
#include "../../src/playlist.h"
#include "../../src/scorewatch.h"
#include "../../src/desk.h"
#include "../../src/weather.h"
#include "../../src/holidays.h"
#include "../../src/feeds.h"
#include "../../src/feeds2.h"
#include "../../src/motion.h"
#include "../../src/astro.h"

Settings CFG;
static std::string FIXTURE;

// stand-in for the HTTPS layer: deserialize the fixture through the same filter
int httpGetJson(const String &, JsonDocument &doc, const JsonDocument *filter, const char *) {
  // same options as src/net.cpp
  DeserializationError e = filter ? deserializeJson(doc, FIXTURE, DeserializationOption::Filter(*filter),
                                                    DeserializationOption::NestingLimit(JSON_NESTING))
                                  : deserializeJson(doc, FIXTURE, DeserializationOption::NestingLimit(JSON_NESTING));
  if (e) printf("fixture JSON error: %s\n", e.c_str());
  return e ? -2 : 200;
}
int httpPostFormJson(const String &, const String &, const String &, JsonDocument &) { return -1; }

static std::string comp(const char *id, const char *ha, const char *abbr, const char *color, int score) {
  std::ostringstream o;
  o << R"({"id":")" << id << R"(","uid":"x","type":"team","order":0,"homeAway":")" << ha
    << R"(","team":{"id":")" << id << R"(","location":"Big City","name":"N","abbreviation":")" << abbr
    << R"(","displayName":"D","color":")" << color << R"(","alternateColor":"a5acaf","logo":"http://x"},"score":")"
    << score << R"(","statistics":[],"records":[{"name":"overall","abbreviation":"Game","type":"total","summary":"5-4"}],"leaders":[{"name":"passingYards","leaders":[{"value":250}]}]})";
  return o.str();
}

static std::string event(const char *id, const char *date, const std::string &away, const std::string &home,
                         const char *state, const char *detail, int period, const char *clock,
                         const char *situation = nullptr) {
  std::ostringstream o;
  o << R"({"id":")" << id << R"(","uid":"u","date":")" << date
    << R"(","name":"x","shortName":"x","season":{"year":2026},"week":{"number":11},"competitions":[{"id":")" << id
    << R"(","attendance":0,"venue":{"fullName":"Stadium"},"competitors":[)" << home << "," << away << "]";
  if (situation) o << R"(,"situation":)" << situation;
  o << R"(,"broadcasts":[{"names":["CBS"]}]}],"links":[],"status":{"clock":0,"displayClock":")" << clock
    << R"(","period":)" << period << R"(,"type":{"id":"2","name":"STATUS","state":")" << state
    << R"(","completed":false,"description":"d","detail":"long","shortDetail":")" << detail << R"("}}})";
  return o.str();
}

static void test_scoreboard_parse() {
  std::string sit = R"({"down":3,"distance":7,"possession":"13","shortDownDistanceText":"3rd & 7","isRedZone":true,"lastPlay":{"text":"blah"}})";
  FIXTURE = R"({"leagues":[{"calendar":[{"label":"Week 1"}]}],"season":{"type":2},"events":[)" +
            event("401", "2026-11-16T21:25Z", comp("7", "away", "DEN", "0a2343", 17), comp("13", "home", "LV", "000000", 21),
                  "in", "4:12 - 3rd", 3, "4:12", sit.c_str()) +
            "," +
            event("402", "2026-11-17T01:20Z", comp("25", "away", "SF", "aa0000", 0), comp("26", "home", "SEA", "002244", 0),
                  "pre", "11/16 - 5:20 PM PST", 0, "0:00") +
            "]}";
  std::vector<Game> g;
  assert(fetchScoreboard("nfl", g));
  assert(g.size() == 2);
  assert(g[0].away.abbr == "DEN" && g[0].home.abbr == "LV");
  assert(g[0].away.score == 17 && g[0].home.score == 21);
  assert(g[0].state == 'i' && g[0].period == 3 && g[0].clock == "4:12");
  assert(g[0].hasSit && g[0].poss == "13" && g[0].down == "3rd & 7" && g[0].redzone);
  assert(g[0].home.color == 0x000000 && g[0].home.alt == 0xa5acaf && g[0].home.record == "5-4");
  assert(g[1].state == 'p' && !g[1].hasSit);
  assert(g[0].start == parseIsoUtc("2026-11-16T21:25Z") && g[0].start > 1700000000);
  puts("ok  scoreboard parse (filtered)");
}

static Game mk(const char *id, const char *a, const char *h, char st, time_t start, int as = 0, int hs = 0,
               const char *lg = "nfl") {
  Game g;
  g.id = id;
  g.league = lg;
  g.away.abbr = a;
  g.home.abbr = h;
  g.state = st;
  g.start = start;
  g.away.score = as;
  g.home.score = hs;
  return g;
}

static void test_playlist() {
  time_t now = parseIsoUtc("2026-11-16T22:00Z");
  std::vector<Game> games = {mk("1", "DEN", "LV", 'i', now - 3000), mk("2", "NYG", "WSH", 'i', now - 3000),
                             mk("3", "KC", "BUF", 'f', parseIsoUtc("2026-11-16T18:00Z")),
                             mk("4", "SF", "SEA", 'p', parseIsoUtc("2026-11-17T01:20Z"))};
  auto cards = buildCards(games, now, {});
  assert(cards[0].key == "nfl:1" && cards[0].pri == PRI_FAV_LIVE);
  auto pl = makePlaylist(cards);
  int fav = 0;
  for (auto &c : pl) fav += c.key == "nfl:1";
  assert(fav == 6 && pl.size() == 9);
  assert(pl[0].key == "nfl:1" && pl[1].key == "nfl:1" && pl[2].key == "nfl:2");
  std::vector<Game> nba = {mk("9", "BOS", "NYK", 'i', now, 0, 0, "nba")};
  assert(buildCards(nba, now, {}).empty());  // non-favorite NBA hidden
  Rotator r;
  r.update(pl);
  r.advance();
  r.advance();
  assert(r.current()->key == "nfl:2");
  r.update(makePlaylist(buildCards(games, now, {})));
  assert(r.current()->key == "nfl:2");  // keeps its place after a refresh
  puts("ok  priorities, takeover mix, rotator");
}

static void test_scorewatch() {
  ScoreWatch w;
  time_t t = 1790000000;
  assert(w.update("nfl", {mk("1", "DEN", "LV", 'i', t, 0, 0)}).empty());
  auto e = w.update("nfl", {mk("1", "DEN", "LV", 'i', t, 0, 6)});
  assert(e.size() == 1 && e[0].label == "TOUCHDOWN" && e[0].sound == "td" && e[0].home);
  assert(shouldCelebrate(e[0]));
  auto e2 = w.update("nfl", {mk("1", "DEN", "LV", 'i', t, 3, 6)});
  assert(e2.size() == 1 && e2[0].label == "FIELD GOAL" && !shouldCelebrate(e2[0]));  // Denver scored
  auto e3 = w.update("mlb", {mk("5", "SD", "LAD", 'i', t, 0, 0, "mlb")});
  assert(e3.empty());
  auto e4 = w.update("mlb", {mk("5", "SD", "LAD", 'i', t, 0, 2, "mlb")});
  assert(e4[0].label == "2 RUNS!" && e4[0].sound == "run" && shouldCelebrate(e4[0]));
  puts("ok  score changes + celebrate rules");
}

static void test_boxscore() {
  FIXTURE = R"({"boxscore":{"teams":[{"team":{"abbreviation":"LAD"},"statistics":[{"name":"x"}]}],"players":[
    {"team":{"id":"19","abbreviation":"LAD"},"statistics":[
      {"type":"batting","names":["H-AB"],"keys":["k"],"labels":["H-AB","AB","R","H","RBI","HR","BB","K","#P","AVG","OBP","SLG"],
       "descriptions":["d"],"athletes":[
         {"active":true,"athlete":{"id":"1","displayName":"Mookie Betts"},"stats":["0-4","4","0","0","0","0","0","2","15",".250",".330",".400"]},
         {"active":true,"athlete":{"id":"2","displayName":"Shohei Ohtani","headshot":{"href":"x"}},"stats":["2-4","4","1","2","3","1","0","1","18",".302",".390",".610"]}],
       "totals":["7-33"]}]},
    {"team":{"abbreviation":"SD"},"statistics":[]}]},"header":{"id":"5"},"plays":[{"text":"long..."}]})";
  Game g = mk("5", "SD", "LAD", 'i', 0, 0, 0, "mlb");
  std::map<String, StatLine> out;
  assert(fetchBoxLines(g, {{"Shohei Ohtani", "mlb", "LAD"}}, out));
  assert(out["Shohei Ohtani"].line == "2-4 1HR 3RBI 1R");
  assert(out["Shohei Ohtani"].slash == ".302/.390/.610");
  assert(out.size() == 1);

  FIXTURE = R"({"boxscore":{"players":[{"team":{"abbreviation":"LV"},"statistics":[
    {"name":"rushing","labels":["CAR","YDS","AVG","TD","LONG"],"athletes":[{"athlete":{"displayName":"Ashton Jeanty"},"stats":["18","96","5.3","1","22"]}]},
    {"name":"receiving","labels":["REC","YDS","AVG","TD","LONG","TGTS"],"athletes":[{"athlete":{"displayName":"Ashton Jeanty"},"stats":["3","21","7.0","0","11","4"]}]}]}]}})";
  Game n = mk("6", "DEN", "LV", 'i', 0);
  out.clear();
  assert(fetchBoxLines(n, {{"Ashton Jeanty", "nfl", "LV"}}, out));
  assert(out["Ashton Jeanty"].line == "18car 96y 1td | 3rec 21y");

  FIXTURE = R"({"boxscore":{"players":[{"team":{"abbreviation":"LAL"},"statistics":[{"labels":["MIN","FG","3PT","FT","OREB","DREB","REB","AST","STL","BLK","TO","PF","+/-","PTS"],
    "athletes":[{"athlete":{"displayName":"LeBron James"},"stats":["34","10-18","2-5","5-6","1","7","8","9","1","0","3","1","+6","27"]}]}]}]}})";
  Game b = mk("7", "LAL", "GS", 'i', 0, 0, 0, "nba");
  out.clear();
  assert(fetchBoxLines(b, {{"LeBron James", "nba", "LAL"}}, out));
  assert(out["LeBron James"].line == "27p 8r 9a 2 3PT");
  puts("ok  box-score stat lines (MLB slash, NFL multi-category, NBA)");
}

static void test_desk() {
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();
  assert(parseIsoUtc("2026-11-12T17:00:00.0000000") == 1794502800);
  assert(parseIsoUtc("garbage") == 0);
  assert(isoUtc(1794502800) == "2026-11-12T17:00:00Z");
  // 10:00 PM Wed Nov 11 in LA = 06:00 UTC Thu Nov 12: countdowns use the LOCAL date
  time_t now = parseIsoUtc("2026-11-12T06:00:00");
  Countdown xmas{"XMAS", 2026, 12, 25};
  assert(daysUntil(xmas, now) == 44);
  Countdown today{"T", 2026, 11, 11};
  assert(daysUntil(today, now) == 0);
  assert(durationText(61) == "2M" && durationText(3600) == "1H" && durationText(3900) == "1H 5M");
  CFG.twelveHour = true;
  assert(clockText(parseIsoUtc("2026-11-12T23:30:00")) == "3:30P");

  // meetings around 9:00 AM local (17:00Z)
  time_t t9 = parseIsoUtc("2026-11-12T17:00:00");
  std::vector<Meeting> ms;
  ms.push_back({"Lunch", t9 + 3 * 3600, t9 + 4 * 3600, "free"});
  ms.push_back({"Holiday", t9 - 9 * 3600, t9 + 15 * 3600, "oof", true});  // all-day: ignored
  ms.push_back({"Standup", t9, t9 + 1800, "busy"});
  ms.push_back({"Maybe", t9 + 3600, t9 + 5400, "tentative"});
  assert(currentMeeting(ms, t9 + 60)->subject == "Standup");
  assert(nextMeeting(ms, t9 + 60)->subject == "Maybe");     // free lunch skipped
  assert(isBusy(ms, t9 + 60, BUSY_AUTO));
  assert(!isBusy(ms, t9 + 3700, BUSY_AUTO));                // tentative doesn't light it
  assert(isBusy(ms, t9 + 3700, BUSY_FORCE) && !isBusy(ms, t9 + 60, BUSY_FREE));
  assert(!currentMeeting(ms, t9 + 2000));

  CFG.countdowns = {xmas, Countdown{"PAST", 2026, 1, 1}};
  NowPlaying np;
  np.playing = true;
  np.title = "Song";
  np.artist = "Band";
  CFG.holidays = "off";  // holiday cards are tested separately
  auto cards = buildDeskCards(ms, {"Call Sam", "MAIL: Budget"}, np, t9 + 60);
  int kinds[16] = {0};
  for (auto &c : cards) kinds[c.kind]++;
  assert(kinds[CARD_MEETING] == 2 && kinds[CARD_TODO] == 1 && kinds[CARD_COUNTDOWN] == 1 && kinds[CARD_MUSIC] == 1);
  for (auto &c : cards) {
    if (c.key == "mtg:now") assert(c.pri == PRI_MEETING_SOON && c.title == "NOW");
    if (c.key == "mtg:next") assert(c.title == "IN 59M" && c.pri == PRI_MEETING_LATER);
    if (c.kind == CARD_TODO) assert(c.title == "TO DO 2" && c.items.size() == 2);
    if (c.kind == CARD_COUNTDOWN) assert(c.num == 43);  // Nov 12 local -> Dec 25
  }
  auto soon = buildDeskCards(ms, {}, NowPlaying(), t9 + 3600 - 600);  // 10 min before "Maybe"
  for (auto &c : soon)
    if (c.key == "mtg:next") assert(c.pri == PRI_MEETING_SOON && c.title == "IN 10M");

  // clock comes back after every 2 cards
  std::vector<Card> five(5);
  for (int i = 0; i < 5; i++) five[i].key = "c" + String(i);
  auto pl = withClock(five, 2);
  const char *want[] = {"clock:0", "c0", "c1", "clock:1", "c2", "c3", "clock:2", "c4"};
  assert(pl.size() == 8);
  for (int i = 0; i < 8; i++) assert(pl[i].key == want[i]);
  assert(withClock({}, 2).size() == 1 && withClock({}, 2)[0].kind == CARD_CLOCK);

  assert(wxKind(0) == WX_CLEAR && wxKind(2) == WX_PARTLY && wxKind(45) == WX_FOG && wxKind(63) == WX_RAIN &&
         wxKind(75) == WX_SNOW && wxKind(95) == WX_STORM && wxKind(81) == WX_RAIN);
  puts("ok  desk clock: calendar math, busy light, countdowns, desk cards, clock rotation");
}

static void test_weather() {
  CFG.lat = 34.05f;
  CFG.lon = -118.24f;
  FIXTURE = R"({"latitude":34.05,"current_units":{"temperature_2m":"°F"},"current":{"time":"2026-11-12T09:00","interval":900,
    "temperature_2m":58.4,"weather_code":2,"is_day":1},"daily":{"time":["2026-11-12"],"temperature_2m_max":[71.6],"temperature_2m_min":[52.2]}})";
  Weather w;
  assert(fetchWeather(w) && w.ok && w.temp == 58 && w.hi == 72 && w.lo == 52 && w.code == 2 && w.day);
  puts("ok  weather (Open-Meteo)");
}

static int findHoliday(const char *shortName) {
  for (int i = 0; i < holidayCount(); i++)
    if (!strcmp(holidayInfo(i).shortName, shortName)) return i;
  return -1;
}
static void expectHoliday(const char *name, int y, int m, int d) {
  int i = findHoliday(name), mm = 0, dd = 0;
  assert(i >= 0);
  bool ok = holidayDate(i, y, mm, dd);
  if (!ok || mm != m || dd != d) {
    printf("FAIL %s %d: got %d/%d want %d/%d\n", name, y, mm, dd, m, d);
    assert(false);
  }
}

static void test_holidays() {
  int m, d;
  easterDate(2025, m, d); assert(m == 4 && d == 20);
  easterDate(2026, m, d); assert(m == 4 && d == 5);
  easterDate(2027, m, d); assert(m == 3 && d == 28);
  expectHoliday("MLK DAY", 2026, 1, 19);
  expectHoliday("PRES. DAY", 2026, 2, 16);
  expectHoliday("SUPER BOWL", 2026, 2, 8);
  expectHoliday("MARDI GRAS", 2026, 2, 17);
  expectHoliday("SPRING FWD", 2026, 3, 8);
  expectHoliday("MOTHER'S", 2026, 5, 10);
  expectHoliday("MEMORIAL", 2026, 5, 25);
  expectHoliday("MEMORIAL", 2027, 5, 31);
  expectHoliday("FATHER'S", 2026, 6, 21);
  expectHoliday("LABOR DAY", 2026, 9, 7);
  expectHoliday("COLUMBUS", 2026, 10, 12);
  expectHoliday("FALL BACK", 2026, 11, 1);
  expectHoliday("ELECTION", 2026, 11, 3);
  expectHoliday("THANKSGIV", 2026, 11, 26);
  expectHoliday("BLACK FRI", 2026, 11, 27);
  expectHoliday("THANKSGIV", 2027, 11, 25);
  assert(!holidayDate(findHoliday("ELECTION"), 2027, m, d));  // odd year

  CFG.holidays = "all";
  CFG.holidaySkip.clear();
  assert(holidayOn(2026, 10, 31) == findHoliday("HALLOWEEN"));
  assert(holidayOn(2026, 10, 30) == -1);
  int away = 0;
  assert(nextHoliday(2026, 11, 12, 21, away) == findHoliday("THANKSGIV") && away == 14);
  assert(nextHoliday(2026, 12, 31, 5, away) == findHoliday("NEW YEAR") && away == 1);  // crosses the year
  CFG.holidays = "federal";
  assert(holidayOn(2026, 10, 31) == -1);                       // Halloween isn't federal
  assert(holidayOn(2026, 11, 11) == findHoliday("VETERANS"));
  CFG.holidays = "all";
  CFG.holidaySkip = {"Groundhog Day"};
  assert(holidayOn(2026, 2, 2) == -1);
  CFG.holidaySkip.clear();
  for (int i = 0; i < holidayCount(); i++) assert(strlen(holidayInfo(i).shortName) <= 10);

  // birthdays: "MM-DD" repeats every year
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();
  time_t now = parseIsoUtc("2026-11-12T20:00:00");
  Countdown bday{"BDAY", 0, 3, 1};
  assert(daysUntil(bday, now) == 109);  // Nov 12 2026 -> Mar 1 2027
  Countdown soon{"X", 0, 11, 20};
  assert(daysUntil(soon, now) == 8);

  // holiday cards: on the day, and a countdown to the next one
  CFG.countdowns.clear();
  auto cards = buildDeskCards({}, {}, NowPlaying(), parseIsoUtc("2026-10-31T19:00:00"));
  bool sawDay = false, sawNext = false;
  for (auto &c : cards) {
    if (c.kind == CARD_HOLIDAY) sawDay = c.text == "HALLOWEEN" && c.title == "HAPPY";
    if (c.kind == CARD_COUNTDOWN && c.key.startsWith("cdh:")) sawNext = c.title == "FALL BACK" && c.num == 1;
  }
  assert(sawDay && sawNext);
  puts("ok  holidays (federal + observances, Easter, nth-weekday rules, skip list), yearly countdowns");
}

static void test_sun_moon() {
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();
  time_t now = parseIsoUtc("2026-11-12T20:00:00");  // noon in Los Angeles
  time_t rise, set;
  assert(sunTimes(34.05f, -118.24f, now, rise, set));
  struct tm r, s;
  localtime_r(&rise, &r);
  localtime_r(&set, &s);
  int rm = r.tm_hour * 60 + r.tm_min, sm = s.tm_hour * 60 + s.tm_min;
  printf("     LA Nov 12 2026: sunrise %d:%02d, sunset %d:%02d\n", r.tm_hour, r.tm_min, s.tm_hour, s.tm_min);
  assert(rm > 6 * 60 + 10 && rm < 6 * 60 + 30);
  assert(sm > 16 * 60 + 40 && sm < 17 * 60);
  // June in LA: sunrise ~5:42, sunset ~8:08 PM (daylight time)
  assert(sunTimes(34.05f, -118.24f, parseIsoUtc("2026-06-21T19:00:00"), rise, set));
  localtime_r(&rise, &r);
  localtime_r(&set, &s);
  assert(r.tm_hour == 5 && s.tm_hour == 20);
  // known moons: new moon at the 2024-04-08 eclipse, full moon 2024-01-25
  float p = moonPhase(parseIsoUtc("2024-04-08T18:21:00"));
  assert(p < 0.04f || p > 0.96f);
  p = moonPhase(parseIsoUtc("2024-01-25T17:54:00"));
  assert(fabsf(p - 0.5f) < 0.04f);
  assert(!strcmp(moonName(0.5f), "FULL MOON") && !strcmp(moonName(0.0f), "NEW MOON") && !strcmp(moonName(0.25f), "1ST QTR"));
  float d, m, y;
  dateProgress(parseIsoUtc("2026-07-02T19:00:00"), d, m, y);  // noon PDT, July 2
  assert(fabsf(d - 0.5f) < 0.01f && fabsf(y - 0.5f) < 0.01f);
  puts("ok  sunrise/sunset, moon phase, date progress");
}

static void test_ufc() {
  // trimmed copy of ESPN's real MMA feed shape (checked 2026-11-12)
  FIXTURE = R"({"leagues":[{"id":"3321","name":"UFC","calendar":[
    {"label":"UFC 333: Old Card","startDate":"2026-11-08T01:00Z","endDate":"2026-11-08T07:59Z","event":{"$ref":"x"}},
    {"label":"UFC 334: Gane vs. Hokit","startDate":"2026-11-15T01:00Z","endDate":"2026-11-15T07:59Z","event":{"$ref":"x"}},
    {"label":"UFC Fight Night: Prochazka vs. Stirling","startDate":"2026-11-21T18:00Z","endDate":"2026-11-22T07:59Z"}]}],
    "events":[{"id":"600057","name":"UFC 334: Gane vs. Hokit","competitions":[{"id":"1"}]}]})";
  std::vector<FightEvent> ev;
  assert(fetchUfc(ev) && ev.size() == 3 && ev[1].label == "UFC 334: Gane vs. Hokit");
  CFG.ufc = true;
  CFG.ufcDays = 10;
  Card c;
  time_t now = parseIsoUtc("2026-11-12T20:00:00");
  assert(ufcCard(ev, now, c) && c.text == "UFC 334: Gane vs. Hokit" && c.pri == PRI_COUNTDOWN);
  assert(ufcCard(ev, parseIsoUtc("2026-11-14T20:00:00"), c) && c.pri == PRI_MEETING_LATER);  // fight day
  assert(ufcCard(ev, parseIsoUtc("2026-11-15T03:00:00"), c) && c.pri == PRI_FAV_PRE);        // live
  assert(ufcCard(ev, parseIsoUtc("2026-11-15T09:00:00"), c) && c.text.startsWith("UFC Fight Night"));
  CFG.ufcDays = 2;
  assert(!ufcCard(ev, now, c));  // too far away
  puts("ok  UFC schedule (ESPN calendar) and next-event card");
}

static void test_batch2() {
  // last play cleanup (real text from WSH @ NYG, 2026-11-12)
  assert(cleanPlay("(Shotgun) J.Daniels pass deep left intended for C.Okonkwo INTERCEPTED by J.Holland at NYG 0. "
                   "Touchback. ** Injury Update: NYG-K.Thibodeaux has returned to the game.") ==
         "J.Daniels pass deep left intended for C.Okonkwo INTERCEPTED by J.Holland at NYG 0. Touchback");
  assert(cleanPlay("(14:32) (No Huddle, Shotgun) B.Nix pass short right to C.Sutton for 8 yards.") ==
         "B.Nix pass short right to C.Sutton for 8 yards");

  // standings: ESPN's real shape (divisions nested under conferences, entries unsorted)
  FIXTURE = R"({"children":[{"name":"American Football Conference","abbreviation":"AFC","children":[
    {"name":"AFC West","standings":{"entries":[
      {"team":{"abbreviation":"LV","displayName":"Las Vegas Raiders"},"stats":[{"name":"wins","displayValue":"2"},{"name":"losses","displayValue":"7"},{"name":"winPercent","displayValue":".222"},{"name":"ties","displayValue":"0"}]},
      {"team":{"abbreviation":"DEN"},"stats":[{"name":"wins","displayValue":"8"},{"name":"losses","displayValue":"2"},{"name":"winPercent","displayValue":".800"}]},
      {"team":{"abbreviation":"KC"},"stats":[{"name":"wins","displayValue":"6"},{"name":"losses","displayValue":"3"},{"name":"winPercent","displayValue":".667"}]},
      {"team":{"abbreviation":"LAC"},"stats":[{"name":"wins","displayValue":"6"},{"name":"losses","displayValue":"4"},{"name":"winPercent","displayValue":".600"}]}]}},
    {"name":"AFC East","standings":{"entries":[{"team":{"abbreviation":"BUF"},"stats":[{"name":"wins","displayValue":"7"},{"name":"losses","displayValue":"1"},{"name":"winPercent","displayValue":".875"}]}]}}]}]})";
  std::vector<StandGroup> g;
  assert(fetchStandings("nfl", g) && g.size() == 2 && g[0].name == "AFC West");
  assert(g[0].rows[0].abbr == "DEN" && g[0].rows[3].abbr == "LV" && g[0].rows[3].wins == 2);
  CFG.favorites = {{"nfl", "LV"}, {"nba", "LAL"}};
  CFG.cardStandings = true;
  time_t nov = parseIsoUtc("2026-11-12T20:00:00");
  auto cards = standingsCards(g, nov);
  assert(cards.size() == 1 && cards[0].title == "AFC WEST" && cards[0].items.size() == 4);
  assert(cards[0].items[3] == "4|LV|2-7|1" && cards[0].items[0] == "1|DEN|8-2|0");
  assert(!inSeason("mlb", nov) && inSeason("nfl", nov) && inSeason("nba", nov));

  // NBA conference (no nesting, 15 teams, sorted by playoff seed): 4-team window around the Lakers
  String nba = R"({"children":[{"name":"Western Conference","standings":{"entries":[)";
  const char *T[] = {"UTAH", "SAC", "LAC", "NO", "PHX", "DEN", "LAL", "OKC", "HOU", "MIN", "DAL", "GS", "MEM", "POR", "SA"};
  const int seed[] = {15, 14, 13, 12, 11, 9, 1, 2, 3, 4, 5, 6, 7, 8, 10};
  for (int i = 0; i < 15; i++) {
    if (i) nba += ",";
    nba += String(R"({"team":{"abbreviation":")") + T[i] + R"("},"stats":[{"name":"playoffSeed","displayValue":")" +
           String(seed[i]) + R"("},{"name":"wins","displayValue":")" + String(16 - seed[i]) +
           R"("},{"name":"losses","displayValue":")" + String(seed[i]) + R"("}]})";
  }
  nba += "]}}]}";
  FIXTURE = nba.c_str();
  std::vector<StandGroup> w;
  assert(fetchStandings("nba", w) && w.size() == 1 && w[0].rows[0].abbr == "LAL" && w[0].rows[14].abbr == "UTAH");
  auto wc = standingsCards(w, nov);
  assert(wc.size() == 1 && wc[0].title == "WEST" && wc[0].items.size() == 4 && wc[0].items[0] == "1|LAL|15-1|1");

  // Teams presence -> busy light + clock label
  String label;
  uint32_t col;
  assert(presenceBusy("Busy", "InACall", label, col) && label == "IN CALL" && col == 0xFF2020);
  assert(presenceBusy("DoNotDisturb", "Presenting", label, col) && label == "PRESENT");
  assert(!presenceBusy("Away", "Away", label, col) && label == "AWAY" && col == 0xFFC000);
  assert(!presenceBusy("Available", "Available", label, col) && label == "");

  // text: curly quotes / accents / dashes -> panel font, word wrap
  assert(foldAscii("Why can’t you hear") == "Why can't you hear");
  assert(foldAscii("Procházka – Stirling") == "Prochazka - Stirling");
  assert(foldAscii("Churyumov–Gerasimenko") == "Churyumov-Gerasimenko");
  auto lines = wrapText("The European Space Agency's lander Philae touched down", 10);
  const char *want[] = {"The", "European", "Space", "Agency's", "lander", "Philae", "touched", "down"};
  assert(lines.size() == 8);
  for (int i = 0; i < 8; i++) assert(lines[i] == want[i]);
  auto l2 = wrapText("a series of challenges amounting to an ordeal", 10);
  assert(l2[0] == "a series" && l2[1] == "of" && l2[2] == "challenges");
  for (auto &l : wrapText("Supercalifragilistic word", 10)) assert(l.length() <= 10);

  // on this day: gentle filter drops death/disaster items
  FIXTURE = R"({"selected":[{"text":"The European Space Agency's lander Philae  touched down on a comet.","year":2014,"pages":[{"title":"x"}]},
    {"text":"American Airlines Flight 587 crashed into residential buildings, killing 265 people.","year":2001}]})";
  std::vector<HistoryEvent> h;
  assert(fetchHistory(11, 12, true, h) && h.size() == 1 && h[0].year == 2014 && h[0].text.indexOf("  ") < 0);
  assert(fetchHistory(11, 12, false, h) && h.size() == 2);

  // word of the day + dad joke (real response shapes)
  FIXTURE = R"({"word":"gauntlet","date":"2026-11-12","partOfSpeech":"noun","pronunciation":"GAWNT-lut","definition":"a series of challenges amounting to an ordeal","example":"..."})";
  WordOfDay wd;
  assert(fetchWord(wd) && wd.word == "gauntlet" && wd.pos == "noun");
  FIXTURE = R"({"id":"TSfFYT7EBd","joke":"Why can’t you hear a pterodactyl go to the bathroom? The p is silent.","status":200})";
  String j;
  assert(fetchJoke(j) && j.startsWith("Why can"));
  // pinning: clock slots share one pin key; select finds cards by it
  std::vector<Card> five2(3);
  five2[0].key = "nfl:1"; five2[1].key = "tetris"; five2[1].kind = CARD_TETRIS; five2[2].key = "st:nfl:AFC West";
  Rotator rr;
  rr.update(withClock(five2, 2));
  assert(rr.select("clock") && rr.current()->kind == CARD_CLOCK && pinKeyOf(*rr.current()) == "clock");
  assert(rr.select("tetris") && rr.current()->key == "tetris" && cardLabel(*rr.current()) == "Tetris clock");
  assert(!rr.select("nope"));
  rr.advance();
  assert(rr.current()->kind == CARD_CLOCK);  // after tetris comes the next clock slot
  // word clock
  auto wc1 = wordClockLines(23, 30);
  assert(wc1.size() == 3 && wc1[0] == "HALF" && wc1[1] == "PAST" && wc1[2] == "ELEVEN");
  auto wc2 = wordClockLines(21, 47);
  assert(wc2[0] == "QUARTER" && wc2[1] == "TILL" && wc2[2] == "TEN");
  auto wc3 = wordClockLines(11, 58);
  assert(wc3.size() == 2 && wc3[0] == "TWELVE" && wc3[1] == "O'CLOCK");
  auto wc4 = wordClockLines(0, 2);
  assert(wc4[0] == "TWELVE" && wc4[1] == "O'CLOCK");
  assert(wordClockLines(14, 37)[0] == "TWENTY-FIVE" && wordClockLines(14, 37)[1] == "TILL" && wordClockLines(14, 37)[2] == "THREE");
  puts("ok  play-by-play text, standings (NFL division, NBA window), Teams presence, text folding/wrap, feeds");
}

static void test_batch3() {
  // accelerometer: the board hangs chip-down, so standing upright reads about -256 on Z
  float gx, gy, gz;
  mapAxes(0, 0, -256, 1, 1, gx, gy, gz);
  assert(gy > 0.99f && fabsf(gx) < 0.01f);
  bool flat = false;
  assert(classifyOrient(gx, gy, gz, 0, flat) == 0 && !flat);
  assert(classifyOrient(0.95f, 0.1f, 0.1f, 0, flat) == 3);    // right edge down
  assert(classifyOrient(-0.95f, 0.1f, 0.1f, 0, flat) == 1);   // left edge down
  assert(classifyOrient(0.0f, -0.98f, 0.1f, 0, flat) == 2);   // upside down
  assert(classifyOrient(0.6f, 0.6f, 0.2f, 3, flat) == 3);     // halfway: keep what we had
  assert(classifyOrient(0.05f, 0.1f, 0.99f, 1, flat) == 1 && flat);
  // OpenTDB url3986 text
  assert(urlDecode("Who%20painted%20%22The%20Starry%20Night%22%3F") == "Who painted \"The Starry Night\"?");
  // Billboard: Thu 2026-11-12 -> 20 years back is Sun 2006-11-12 -> its chart is dated Sat 2006-11-11
  time_t t = parseIsoUtc("2026-11-12T20:00:00");
  assert(chartDateFor(t, 20) == "2006-11-11");
  assert(chartDateFor(parseIsoUtc("2026-11-14T20:00:00"), 30) == "1996-11-09");
  // surf (Open-Meteo marine)
  {
    JsonDocument d;
    deserializeJson(d, R"({"current":{"wave_height":1.20,"wave_period":10.95,"wave_direction":254,"sea_surface_temperature":23.1},"daily":{"wave_height_max":[1.5]}})");
    SurfReport r;
    assert(parseSurf(d, r) && r.ok && fabsf(r.waveFt - 3.94f) < 0.05f && r.waterF == 74 && r.dirDeg == 254);
  }
  // ESPN team schedule: newest completed game, from our side
  {
    JsonDocument d;
    deserializeJson(d, R"({"events":[
      {"date":"2026-09-26T02:10Z","competitions":[{"status":{"type":{"completed":true}},"competitors":[
        {"team":{"abbreviation":"LAD"},"winner":false,"score":{"value":2}},{"team":{"abbreviation":"SF"},"winner":true,"score":{"value":3}}]}]},
      {"date":"2026-09-27T19:05Z","competitions":[{"status":{"type":{"completed":true}},"competitors":[
        {"team":{"abbreviation":"SF"},"winner":false,"score":{"value":1}},{"team":{"abbreviation":"LAD"},"winner":true,"score":{"value":5}}]}]},
      {"date":"2026-09-28T19:05Z","competitions":[{"status":{"type":{"completed":false}},"competitors":[]}]}]})");
    GameResult r;
    assert(parseLastResult(d, "LAD", parseIsoUtc("2026-09-28T03:00:00"), r));
    assert(r.won && r.us == "LAD" && r.them == "SF" && r.usScore == 5 && r.themScore == 1);
  }
  // astronomy: the June solstice sun sits over the Tropic of Cancer; Jupiter at its Dec 2024 opposition stood high
  // in the south from LA at local midnight
  double sl, so;
  sunSubpoint(parseIsoUtc("2024-06-20T12:00:00"), sl, so);
  assert(fabs(sl - 23.44) < 0.2 && fabs(so) < 2);
  std::vector<PlanetPos> pp;
  planetPositions(parseIsoUtc("2024-12-07T08:00:00"), 34.05, -118.24, pp);
  assert(pp.size() == 5 && pp[3].alt > 70 && pp[3].az > 160 && pp[3].az < 210);
  assert(String(compass8(250)) == "W" && String(compass8(10)) == "N" && String(compass8(225)) == "SW");
  puts("ok  accelerometer orientation, trivia text, Billboard dates, surf, Fly the W, planets & sun");
}

int main() {
  CFG.favorites = {{"mlb", "LAD"}, {"nfl", "LV"}, {"nba", "LAL"}};
  CFG.showAll = {"nfl"};
  CFG.celebrate = {"nfl", "mlb"};
  test_scoreboard_parse();
  test_playlist();
  test_scorewatch();
  test_boxscore();
  test_desk();
  test_weather();
  test_holidays();
  test_sun_moon();
  test_ufc();
  test_batch2();
  test_batch3();
  puts("all C++ logic tests passed");
}
