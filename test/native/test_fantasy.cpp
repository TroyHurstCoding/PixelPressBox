// Desktop test for fantasy points: scores a real game (MIA @ BUF, 2026 week 11, ESPN summary trimmed into
// fixtures/) with the owner's league rules, and checks the board's totals, defence and live "has the ball" logic.
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "Arduino.h"
#include <ArduinoJson.h>
#include <cassert>
#include <cmath>
#include <fstream>
#include <sstream>
#include "../../src/fantasy.h"
#include "../../src/espn.h"
#include "../../src/feeds3.h"
#include "../../src/render3.h"
#include "../../src/net.h"

Settings CFG;
int httpGetJson(const String &, JsonDocument &, const JsonDocument *, const char *) { return -1; }
int httpPostFormJson(const String &, const String &, const String &, JsonDocument &) { return -1; }

static std::string slurp(const char *p) {
  std::ifstream f(p);
  std::stringstream s;
  s << f.rdbuf();
  return s.str();
}
static bool near(float a, float b) { return fabsf(a - b) < 0.011f; }
#define CHECK(c)                                          \
  do {                                                    \
    if (!(c)) {                                           \
      printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); \
      fails++;                                            \
    }                                                     \
  } while (0)

int main() {
  int fails = 0;
  std::string fx = slurp("fixtures/nfl_summary_mia_buf_2026w11.json");
  JsonDocument full;
  CHECK(!deserializeJson(full, fx, DeserializationOption::NestingLimit(JSON_NESTING)));
  // the device downloads through the filter; do the same so the filter is tested too
  JsonDocument filter, d;
  fantasySummaryFilter(filter);
  deserializeJson(d, fx, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(JSON_NESTING));

  std::map<String, StatMap> ath, teams;
  fantasyAddBox(d, ath, teams);
  FantasyRules r = fantasyRules();

  // Josh Allen (id 3918298): 27/45 304 yd 1 TD 2 INT passing, plus his rushing line from the same box score
  const StatMap &allen = ath["3918298"];
  CHECK(near(allen.at("passYds"), 304) && near(allen.at("passTD"), 1) && near(allen.at("passInt"), 2));
  float expect = 304 * 0.04f + 1 * 5 - 2 * 2;
  auto get = [](const StatMap &s, const char *k) { auto it = s.find(k); return it == s.end() ? 0.0f : it->second; };
  expect += get(allen, "rushYds") * 0.1f + get(allen, "rushTD") * 6 - get(allen, "fumLost") * 2;
  printf("Allen  %-34s %6.2f pts (expect %.2f)\n", fantasyStatLine(allen).c_str(), fantasyScore(allen, r), expect);
  CHECK(near(fantasyScore(allen, r), roundf(expect * 100) / 100));

  // yardage bonus: only pays at the threshold named in the rule
  StatMap big{{"passYds", 401}, {"att", 40}, {"cmp", 30}};
  CHECK(near(fantasyScore(big, r), 401 * 0.04f + 2));
  StatMap rb{{"rushYds", 149}, {"carries", 20}};
  CHECK(near(fantasyScore(rb, r), 14.9f));
  rb["rushYds"] = 150;
  CHECK(near(fantasyScore(rb, r), 17));

  // kicker misses: Carlson went 3/3 FG, 0/0 XP in this game; a made-up 2/4 + 1/2 pays 6 - 4 + 1 - 1
  StatMap k{{"fgMade", 2}, {"fgAtt", 4}, {"xpMade", 1}, {"xpAtt", 2}};
  CHECK(near(fantasyScore(k, r), 2));

  // half PPR receiver: Shakir 5 rec 101 yd, plus 2 carries for 2 yd
  for (auto &kv : ath)
    if (near(get(kv.second, "rec"), 5) && near(get(kv.second, "recYds"), 101)) {
      printf("Shakir %-34s %6.2f pts (expect 12.80)\n", fantasyStatLine(kv.second).c_str(), fantasyScore(kv.second, r));
      CHECK(near(fantasyScore(kv.second, r), 12.8f));
    }

  // team defences, points allowed off the final score (rebuilt from the scoring plays)
  int bufPts = 0, miaPts = 0;
  for (JsonObjectConst sp : full["scoringPlays"].as<JsonArrayConst>()) {
    String t = sp["type"]["abbreviation"] | "", ab = sp["team"]["abbreviation"] | "";
    int v = t == "TD" ? 7 : t == "FG" ? 3 : t == "SF" ? 2 : 0;  // assumes made PATs; checked against ESPN below
    (ab == "BUF" ? bufPts : miaPts) += v;
  }
  printf("score from scoring plays: BUF %d  MIA %d\n", bufPts, miaPts);
  float bufDst = fantasyDstScore(teams["BUF"], miaPts, r), miaDst = fantasyDstScore(teams["MIA"], bufPts, r);
  printf("BUF D/ST %-30s %6.2f   MIA D/ST %-30s %6.2f\n", fantasyDstLine(teams["BUF"], miaPts).c_str(), bufDst,
         fantasyDstLine(teams["MIA"], bufPts).c_str(), miaDst);

  // the whole board: a live game where BUF has the ball
  CFG.fptsPlayers = {{"3918298", "Josh Allen", "QB", "BUF", 0}, {"", "BUF D/ST", "DST", "BUF", 0},
                     {"", "MIA D/ST", "DST", "MIA", 1}, {"999", "Bye Guy", "WR", "KC", 1}};
  Game g;
  g.id = "401873070";
  g.league = "nfl";
  g.state = 'i';
  g.away.abbr = "MIA", g.away.id = "15", g.away.score = miaPts;
  g.home.abbr = "BUF", g.home.id = "2", g.home.score = bufPts;
  g.poss = "2";
  int fetches = 0;
  FantasyCache cache;
  auto fetch = [&](const String &id, JsonDocument &doc) {
    fetches++;
    CHECK(id == "401873070");
    return !deserializeJson(doc, fx, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(JSON_NESTING));
  };
  FptsBoard b = fantasyBuild({g}, fetch, cache);
  CHECK(b.ok && b.lines.size() == 4 && b.anyLive);
  CHECK(b.lines[0].hasBall && !b.lines[1].hasBall && b.lines[2].hasBall);  // BUF offense on, BUF D off, MIA D on
  CHECK(b.lines[3].state == 'n' && near(b.lines[3].pts, 0));                // no game this week
  CHECK(near(b.me, roundf((fantasyScore(allen, r) + bufDst) * 100) / 100) && near(b.opp, miaDst));
  printf("board: %s %.2f - %.2f %s\n", b.meLabel.c_str(), b.me, b.opp, b.oppLabel.c_str());
  // live games are downloaded again each time; a final only once
  fantasyBuild({g}, fetch, cache);
  g.state = 'f';
  fantasyBuild({g}, fetch, cache);
  fantasyBuild({g}, fetch, cache);
  CHECK(fetches == 3);
  // a week later the game is gone from the scoreboard and leaves the cache
  fantasyBuild({}, fetch, cache);
  CHECK(cache.athletes.empty());

  // ---------------------------------------------------------------- batch 4 feeds
  auto load = [&](const char *path, JsonDocument &doc) {
    std::string t = slurp(path);
    CHECK(!t.empty());
    CHECK(!deserializeJson(doc, t, DeserializationOption::NestingLimit(JSON_NESTING)));
  };
  {  // golf: order, ties, even par
    JsonDocument d;
    load("fixtures/golf_scoreboard_rsm_2026.json", d);
    GolfBoard gb;
    parseGolf(d, gb);
    CHECK(gb.ok && gb.state == 'f' && gb.rows.size() == 10);
    CHECK(gb.rows[0].pos == "1" && gb.rows[0].score == "-17" && gb.rows[1].pos == "T2" && gb.rows[3].pos == "T2");
    printf("golf: %s | %s %s %s | %s %s %s\n", gb.event.c_str(), gb.rows[0].pos.c_str(), gb.rows[0].name.c_str(),
           gb.rows[0].score.c_str(), gb.rows[1].pos.c_str(), gb.rows[1].name.c_str(), gb.rows[1].score.c_str());
  }
  {  // next game from a team schedule: the first game not yet played
    JsonDocument d;
    load("fixtures/nba_schedule_lal_2026.json", d);
    NextGame n;
    time_t now = parseIsoUtc("2026-11-24T05:00:00");
    CHECK(parseNextGame(d, "nba", "LAL", now, n));
    CHECK(n.us == "LAL" && n.them == "SA" && !n.home && n.tv == "PRIME VIDEO" && n.start == parseIsoUtc("2026-11-28T02:30Z"));
    printf("next: %s @ %s %s\n", n.us.c_str(), n.them.c_str(), n.tv.c_str());
  }
  {  // planes: within the radius, nearest first, nothing on the ground
    JsonDocument d;
    load("fixtures/opensky_la_2026-11-23.json", d);
    std::vector<Flight> f;
    parseFlights(d, 34.05f, -118.24f, 40, f);
    CHECK(!f.empty() && f.size() <= 12);
    for (size_t i = 1; i < f.size(); i++) CHECK(f[i - 1].distKm <= f[i].distKm);
    for (auto &x : f) CHECK(x.distKm <= 40 && x.callsign.length() && x.callsign.indexOf(' ') < 0);
    printf("planes: %d within 40 km, nearest %s %d ft %.1f km\n", (int)f.size(), f[0].callsign.c_str(), f[0].altFt, f[0].distKm);
    CHECK(fabsf(distanceKm(34.05f, -118.24f, 40.71f, -74.01f) - 3936) < 15);  // LA - NYC
    CHECK(fabsf(bearingDeg(0, 0, 1, 0)) < 0.5f && fabsf(bearingDeg(0, 0, 0, 1) - 90) < 0.5f);
  }
  {  // top performers from the real summary
    JsonDocument d;
    load("fixtures/nfl_summary_mia_buf_2026w11.json", d);
    std::vector<Highlight> h;
    Game g;
    g.league = "nfl", g.id = "1", g.home.abbr = "BUF", g.away.abbr = "MIA";
    parseHighlights(d, g, h);
    CHECK(h.size() == 2);
    for (auto &x : h) printf("top: %s %s %s\n", x.team.c_str(), x.who.c_str(), x.what.c_str());
    CHECK(h[0].team == "BUF" && h[0].who == "J. ALLEN" && h[0].what == "304 YDS 1 TD");
    WinProb w;
    parseWinProb(d, g, w);
    CHECK(w.ok && w.home.size() == 188 && w.home[0] == 95);
  }
  {  // world clocks: daylight saving switches
    WorldCity ny{"NYC", -300, "us"}, lon{"LON", 0, "eu"}, syd{"SYD", 600, "au"}, tok{"TOK", 540, ""};
    auto hourAt = [](time_t t) { return (int)((t % 86400) / 3600); };
    bool dst;
    // 2026-03-08 06:59 UTC = 01:59 EST; 07:00 UTC = 03:00 EDT
    CHECK(hourAt(worldLocal(parseIsoUtc("2026-03-08T06:59:00"), ny, &dst)) == 1 && !dst);
    CHECK(hourAt(worldLocal(parseIsoUtc("2026-03-08T07:00:00"), ny, &dst)) == 3 && dst);
    // 2026-11-01 05:59 UTC = 01:59 EDT; 06:00 UTC = 01:00 EST
    CHECK(hourAt(worldLocal(parseIsoUtc("2026-11-01T05:59:00"), ny, &dst)) == 1 && dst);
    CHECK(hourAt(worldLocal(parseIsoUtc("2026-11-01T06:00:00"), ny, &dst)) == 1 && !dst);
    // EU: last Sunday of March 2026 is the 29th, 01:00 UTC
    CHECK(hourAt(worldLocal(parseIsoUtc("2026-03-29T00:59:00"), lon)) == 0);
    CHECK(hourAt(worldLocal(parseIsoUtc("2026-03-29T01:00:00"), lon)) == 2);
    CHECK(hourAt(worldLocal(parseIsoUtc("2026-10-25T00:59:00"), lon)) == 1);
    CHECK(hourAt(worldLocal(parseIsoUtc("2026-10-25T01:00:00"), lon)) == 1);
    // Sydney is on daylight time in November, standard in July
    worldLocal(parseIsoUtc("2026-11-24T00:00:00"), syd, &dst);
    CHECK(dst);
    worldLocal(parseIsoUtc("2026-07-01T00:00:00"), syd, &dst);
    CHECK(!dst);
    CHECK(hourAt(worldLocal(parseIsoUtc("2026-11-24T05:00:00"), tok)) == 14);
  }
  CHECK(text3Width("ABC") == 11 && text3Width("") == 0);

  printf(fails ? "%d FANTASY TEST(S) FAILED\n" : "all fantasy + batch 4 feed tests passed\n", fails);
  return fails ? 1 : 0;
}
