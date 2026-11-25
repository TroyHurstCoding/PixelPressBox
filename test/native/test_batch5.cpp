// Desktop test for batch 5 feeds (src/feeds5.cpp): launches, park waits, stocks, news, drive time, Powerball,
// pick'em and the playoff line. Fixtures in fixtures/b5_* are REAL saved responses (trimmed), except
// b5_tomtom_*_sample.json which are HAND-MADE in the documented TomTom response shape (no API key available).
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "Arduino.h"
#include <ArduinoJson.h>
#include <cmath>
#include <fstream>
#include <sstream>
#include "../../src/feeds5.h"
#include "../../src/espn.h"
#include "../../src/net.h"

Settings CFG;   // the firmware global some linked sources refer to
static int fails = 0;
#define CHECK(c)                                          \
  do {                                                    \
    if (!(c)) {                                           \
      printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); \
      fails++;                                            \
    }                                                     \
  } while (0)

static std::string slurp(const char *p) {
  std::ifstream f(p);
  std::stringstream s;
  s << f.rdbuf();
  return s.str();
}
static bool load(const char *p, JsonDocument &d) {
  std::string t = slurp(p);
  return !t.empty() && !deserializeJson(d, t, DeserializationOption::NestingLimit(JSON_NESTING));
}
static bool near(float a, float b, float e = 0.01f) { return fabsf(a - b) < e; }
static time_t utc(int y, int mo, int d, int h = 0, int mi = 0, int s = 0) {
  char b[40];
  snprintf(b, sizeof b, "%04d-%02d-%02dT%02d:%02d:%02dZ", y, mo, d, h, mi, s);
  return parseIsoUtc(b);
}

static void testLaunches() {
  JsonDocument d;
  CHECK(load("fixtures/b5_launches_normal.json", d));
  std::vector<Launch> v;
  parseLaunches(d, v);
  // 5 real rows: the first three already flew ("Success"), so two remain, soonest first
  CHECK(v.size() == 2);
  if (v.size() == 2) {
    CHECK(v[0].name == "Long March 2F/G | Shenzhou 24");
    CHECK(v[0].rocket == "Long March 2F/G");
    CHECK(v[0].mission == "Shenzhou 24");
    CHECK(v[0].provider == "CASC");
    CHECK(v[0].status == "Go");
    CHECK(v[0].pad.indexOf("Launch Area 91") == 0);
    CHECK(v[0].net == utc(2026, 11, 25, 0, 18, 0));
    CHECK(!v[0].vandenberg);
    CHECK(v[0].id.length() == 36);
    CHECK(v[1].rocket == "Long March 2D" && v[0].net < v[1].net && v[1].mission == "Unknown Payload");
  }
  // Vandenberg + list-mode style (no rocket/mission/provider objects) + out of order
  JsonDocument j;
  deserializeJson(j, R"({"results":[
    {"id":"b","name":"Falcon 9 Block 5 | Starlink 11-5","status":{"abbrev":"TBC"},"net":"2026-12-02T03:00:00Z"},
    {"id":"a","name":"Falcon 9 Block 5 | Transporter-20","status":{"abbrev":"Go"},"net":"2026-12-01T18:00:00Z",
     "pad":{"name":"Space Launch Complex 4E","location":{"name":"Vandenberg SFB, CA, USA"}},
     "launch_service_provider":{"name":"SpaceX"}}]})");
  parseLaunches(j, v);
  CHECK(v.size() == 2 && v[0].id == "a" && v[1].id == "b");
  CHECK(v[0].vandenberg && v[0].provider == "SpaceX" && v[0].pad == "Space Launch Complex 4E");
  CHECK(v[1].rocket == "Falcon 9 Block 5" && v[1].mission == "Starlink 11-5" && !v[1].vandenberg);
  // a pad called "Unknown Pad" falls back to the location
  deserializeJson(j, R"({"results":[{"id":"x","name":"Electron | StriX","status":{"abbrev":"Go"},"net":"2026-11-27T00:05:00Z",
     "pad":{"name":"Unknown Pad","location":{"name":"Rocket Lab LC-1, Mahia, New Zealand"}}}]})");
  parseLaunches(j, v);
  CHECK(v.size() == 1 && v[0].pad == "Rocket Lab LC-1, Mahia, New Zealand");
  JsonDocument e;
  deserializeJson(e, "{\"results\":[]}");
  parseLaunches(e, v);
  CHECK(v.empty());
  deserializeJson(e, "{}");
  v.push_back(Launch());
  parseLaunches(e, v);
  CHECK(v.empty());
}

static void testWaits() {
  JsonDocument d;
  CHECK(load("fixtures/b5_queuetimes_16.json", d));
  ParkWaits p;
  parseParkWaits(d, p);
  CHECK(p.ok && p.rides.size() > 10);
  bool sorted = true, allOpen = true;
  for (size_t i = 0; i < p.rides.size(); i++) {
    if (i && p.rides[i].wait > p.rides[i - 1].wait) sorted = false;
    if (!p.rides[i].open) allOpen = false;
  }
  CHECK(sorted && allOpen);
  CHECK(p.rides[0].wait >= 30);
  // names are ASCII (the real feed has "Indiana Jones(TM) Adventure" and curly apostrophes)
  bool ascii = true;
  for (auto &r : p.rides)
    for (size_t i = 0; i < r.name.length(); i++)
      if ((unsigned char)r.name[i] > 126) ascii = false;
  CHECK(ascii);
  // closed rides are dropped; a ride listed under a land and at the top level counts once
  JsonDocument j;
  deserializeJson(j, R"({"lands":[{"id":1,"name":"L","rides":[
     {"id":10,"name":"A","is_open":true,"wait_time":15},{"id":11,"name":"B","is_open":false,"wait_time":0}]}],
     "rides":[{"id":10,"name":"A","is_open":true,"wait_time":15},{"id":12,"name":"C","is_open":true,"wait_time":50}]})");
  parseParkWaits(j, p);
  CHECK(p.ok && p.rides.size() == 2 && p.rides[0].name == "C" && p.rides[0].wait == 50 && p.rides[1].name == "A");
  deserializeJson(j, R"({"lands":[],"rides":[]})");
  parseParkWaits(j, p);
  CHECK(p.ok && p.rides.empty());
  deserializeJson(j, "{}");
  parseParkWaits(j, p);
  CHECK(!p.ok);
}

static void testStocks() {
  JsonDocument d;
  CHECK(load("fixtures/b5_robinhood_price_aapl.json", d));
  StockQuote q;
  q.symbol = "AAPL";
  time_t now = utc(2026, 11, 24, 22, 18, 30);   // 5:18 PM ET Nov 24
  CHECK(parseStock(d, q, now));
  CHECK(q.ok && near(q.price, 354.00f) && near(q.bid, 353.92f) && near(q.ask, 354.08f));
  CHECK(near(q.dayHigh, 354.37f) && near(q.dayLow, 345.88f) && !q.halted && q.at == now);
  CHECK(near(q.open, 354.00f) && q.spark.size() == 1);
  // second tick the same day keeps open, appends to spark
  JsonDocument d2;
  deserializeJson(d2, R"({"quotes":[{"bid":"355.00","ask":"355.50","dailyHigh":"355.5","dailyLow":"345.88","isTradingHalt":true}]})");
  CHECK(parseStock(d2, q, now + 15));
  CHECK(near(q.price, 355.25f) && near(q.open, 354.0f) && q.spark.size() == 2 && q.halted);
  // garbage / empty leaves the quote alone
  JsonDocument bad;
  deserializeJson(bad, R"({"quotes":[]})");
  CHECK(!parseStock(bad, q, now + 30) && q.spark.size() == 2 && near(q.price, 355.25f));
  deserializeJson(bad, R"({"quotes":[{"bid":"","ask":""}]})");
  CHECK(!parseStock(bad, q, now + 30));

  // rollover across ET midnight: 11:59 PM ET (04:59 UTC next day, EST) vs 12:01 AM ET
  StockQuote r;
  r.price = 100;
  r.open = 98;
  r.spark = {98, 99, 100};
  r.at = utc(2026, 11, 25, 4, 59);   // Nov 24 23:59 ET
  stockRollover(r, utc(2026, 11, 25, 4, 59, 59));
  CHECK(r.spark.size() == 3 && r.open == 98 && r.prevClose == 0);   // still the same ET day
  stockRollover(r, utc(2026, 11, 25, 5, 1));                        // 00:01 ET Nov 25
  CHECK(r.spark.empty() && r.open == 0 && near(r.prevClose, 100));
  // UTC midnight is NOT the rollover (7 PM ET): same ET day
  StockQuote s;
  s.price = 50;
  s.spark = {50};
  s.at = utc(2026, 11, 24, 23, 0);
  stockRollover(s, utc(2026, 11, 25, 0, 30));
  CHECK(s.spark.size() == 1 && s.prevClose == 0);
  // in summer (EDT) midnight ET is 04:00 UTC
  StockQuote t;
  t.price = 70;
  t.spark = {70};
  t.at = utc(2026, 7, 10, 3, 59);
  stockRollover(t, utc(2026, 7, 10, 3, 59, 59));
  CHECK(t.spark.size() == 1);
  stockRollover(t, utc(2026, 7, 10, 4, 0, 1));
  CHECK(t.spark.empty() && near(t.prevClose, 70));
  // first ever quote (at == 0) never rolls
  StockQuote u;
  u.price = 5;
  u.spark = {5};
  stockRollover(u, now);
  CHECK(u.spark.size() == 1);
  // parseStock itself rolls over: yesterday's quote becomes prevClose
  StockQuote w;
  w.price = 300;
  w.open = 299;
  w.spark = {299, 300};
  w.at = utc(2026, 11, 23, 21, 0);
  CHECK(parseStock(d, w, now));
  CHECK(near(w.prevClose, 300) && near(w.open, 354.0f) && w.spark.size() == 1);

  // spark thinning: 64 samples + 1 -> every other one kept (plus the newest), whole day's shape stays
  StockQuote k;
  k.at = now;
  for (int i = 0; i < 64; i++) k.spark.push_back((float)i);
  CHECK(parseStock(d, k, now + 10));
  CHECK(k.spark.size() == 34);   // 32 even samples 0..62, the newest old one (63), and the new price
  CHECK(k.spark[0] == 0 && k.spark[1] == 2 && k.spark[31] == 62 && k.spark[32] == 63 && near(k.spark[33], 354.0f));
  for (int i = 0; i < 200; i++) parseStock(d, k, now + 20 + i);
  CHECK(k.spark.size() <= 64 && k.spark.size() > 30);
  CHECK(k.spark[0] == 0);   // the day's first sample survives every thinning

  // asset list
  JsonDocument a;
  CHECK(load("fixtures/b5_robinhood_assets.json", a));
  std::vector<std::pair<String, String>> l;
  parseStockList(a, l);
  CHECK(l.size() == 10);
  bool ord = true;
  for (size_t i = 1; i < l.size(); i++)
    if (!(l[i - 1].first < l[i].first)) ord = false;
  CHECK(ord);
  bool found = false;
  for (auto &p : l)
    if (p.first == "CRM") {
      found = true;
      CHECK(p.second == "Salesforce");
    }
  CHECK(found);
  JsonDocument ea;
  deserializeJson(ea, "{}");
  parseStockList(ea, l);
  CHECK(l.empty());
}

static void testNews() {
  JsonDocument d;
  CHECK(load("fixtures/b5_espn_news_nfl.json", d));
  std::vector<NewsItem> v;
  NewsWatch w;
  // team tag (DEN): the first article is tagged Denver and mentions "suspended"
  w.teams = {{"nfl", "DEN"}};
  parseNews(d, "nfl", w, v);
  CHECK(v.size() >= 1);
  if (v.size()) {
    CHECK(v[0].id == "50586087" && v[0].league == "nfl" && v[0].why == "DEN");
    CHECK(v[0].published == utc(2026, 11, 24, 22, 13, 7));
    CHECK(v[0].headline.indexOf("Broncos' Sean Payton") == 0);
  }
  // keyword + team: the label becomes the keyword
  w.keywords = {"suspended"};
  parseNews(d, "nfl", w, v);
  CHECK(v.size() >= 1 && v[0].why == "SUSPENDED");
  // watched team of another league does not apply to this feed
  NewsWatch other;
  other.teams = {{"nba", "DEN"}};
  parseNews(d, "nfl", other, v);
  CHECK(v.empty());
  // athlete by ESPN id (Jordan Love 4036378) -> short name
  NewsWatch aw;
  aw.athleteIds = {"4036378"};
  parseNews(d, "nfl", aw, v);
  CHECK(v.size() >= 1);
  bool love = false;
  for (auto &n : v)
    if (n.why == "J.LOVE") love = true;
  CHECK(love);
  // athlete by name (tag description), case-insensitive
  NewsWatch pw;
  pw.playerNames = {"ashton jeanty"};
  parseNews(d, "nfl", pw, v);
  CHECK(v.size() >= 1 && v[0].why == "A.JEANTY");
  // nothing watched, or only a keyword: no match
  NewsWatch none;
  none.keywords = {"trade", "injury", "suspended"};
  parseNews(d, "nfl", none, v);
  CHECK(v.empty());
  NewsWatch nomatch;
  nomatch.teams = {{"nfl", "ZZZ"}};
  nomatch.athleteIds = {"1"};
  nomatch.playerNames = {"Nobody Atall"};
  parseNews(d, "nfl", nomatch, v);
  CHECK(v.empty());
  // hand-made: curly quotes folded, numeric article id, player named in the headline only, sorted newest first
  JsonDocument j;
  deserializeJson(j, R"({"articles":[
    {"id":1,"headline":"Bills ’ Allen — out for the week","published":"2026-11-20T10:00:00Z","categories":[]},
    {"dataSourceIdentifier":"abc","headline":"Josh Allen throws 5 TD","published":"2026-11-21T10:00:00Z","categories":[]},
    {"id":"3","headline":"Bills roster moves","published":"2026-11-22T10:00:00Z",
     "categories":[{"type":"team","description":"Buffalo Bills","team":{"abbreviation":"BUF"}}]}]})");
  NewsWatch h;
  h.teams = {{"nfl", "buf"}};
  h.playerNames = {"Josh Allen"};
  h.keywords = {"out for"};
  parseNews(j, "nfl", h, v);
  CHECK(v.size() == 2);   // article 1 names neither the team tag nor the full name
  if (v.size() == 2) {
    CHECK(v[0].id == "3" && v[0].why == "BUF");
    CHECK(v[1].id == "abc" && v[1].why == "J.ALLEN");
  }
  h.playerNames = {"Allen"};
  parseNews(j, "nfl", h, v);
  CHECK(v.size() == 3 && v[2].id == "1" && v[2].why == "OUT FOR");
  CHECK(v[2].headline == "Bills ' Allen - out for the week");
  deserializeJson(j, "{}");
  parseNews(j, "nfl", h, v);
  CHECK(v.empty());
}

static void testDrive() {
  // hand-made fixtures in TomTom's documented shape (no key available)
  JsonDocument d;
  CHECK(load("fixtures/b5_tomtom_route_sample.json", d));
  DriveTime t;
  t.label = "WORK";
  parseDriveTime(d, t);
  CHECK(t.ok && t.minutes == 34 && t.typical == 26 && t.delay == 8 && near(t.km, 22.5f, 0.05f) && t.label == "WORK");
  JsonDocument e;
  deserializeJson(e, R"({"routes":[]})");
  parseDriveTime(e, t);
  CHECK(!t.ok);
  deserializeJson(e, R"({"error":{"description":"bad key"}})");
  parseDriveTime(e, t);
  CHECK(!t.ok);
  JsonDocument g;
  CHECK(load("fixtures/b5_tomtom_geocode_sample.json", g));
  float lat = 0, lon = 0;
  CHECK(parseGeocode(g, lat, lon) && near(lat, 34.0522f, 0.0001f) && near(lon, -118.2437f, 0.0001f));
  deserializeJson(g, R"({"results":[]})");
  lat = 1;
  CHECK(!parseGeocode(g, lat, lon) && lat == 1);
  deserializeJson(g, R"({"results":[{"position":{"lat":1.5}}]})");
  CHECK(!parseGeocode(g, lat, lon));
}

static void testLotto() {
  JsonDocument d;
  CHECK(load("fixtures/b5_powerball_ny.json", d));
  LottoDraw l;
  parseLotto(d, l);
  CHECK(l.ok && l.drawDate == utc(2026, 11, 23) && l.white[0] == 7 && l.white[1] == 9 && l.white[2] == 27 &&
        l.white[3] == 40 && l.white[4] == 58 && l.power == 19 && l.multiplier == 2 && l.jackpot == "");
  JsonDocument e;
  deserializeJson(e, "[]");
  parseLotto(e, l);
  CHECK(!l.ok);
  deserializeJson(e, R"([{"draw_date":"2026-11-23T00:00:00.000","winning_numbers":"01 02 03"}])");
  parseLotto(e, l);
  CHECK(!l.ok);

  // next draw: Mon/Wed/Sat 22:59 ET. Mon 2026-11-23 22:59 EST = Tue 03:59 UTC
  CHECK(nextPowerballDraw(utc(2026, 11, 23, 12)) == utc(2026, 11, 24, 3, 59));
  CHECK(nextPowerballDraw(utc(2026, 11, 24, 3, 30)) == utc(2026, 11, 24, 3, 59));        // 10:30 PM Monday ET
  CHECK(nextPowerballDraw(utc(2026, 11, 24, 3, 59)) == utc(2026, 11, 26, 3, 59));        // at the draw: the next one
  CHECK(nextPowerballDraw(utc(2026, 11, 25, 3, 0)) == utc(2026, 11, 26, 3, 59));         // Tue 10 PM ET -> Wed draw
  CHECK(nextPowerballDraw(utc(2026, 11, 28, 12)) == utc(2026, 11, 29, 3, 59));           // Saturday
  CHECK(nextPowerballDraw(utc(2026, 11, 29, 12)) == utc(2026, 12, 1, 3, 59));            // Sunday -> Monday (weekend)
  CHECK(nextPowerballDraw(utc(2026, 11, 30, 2, 0)) == utc(2026, 12, 1, 3, 59));          // Sun 9 PM ET -> Monday
  // DST: clocks spring forward Sun 2027-03-14; Sat draw is still EST, Monday's is EDT (02:59 UTC)
  CHECK(nextPowerballDraw(utc(2027, 3, 13, 12)) == utc(2027, 3, 14, 3, 59));
  CHECK(nextPowerballDraw(utc(2027, 3, 14, 12)) == utc(2027, 3, 16, 2, 59));
  // and back on Sun 2027-11-07: Sat draw is EDT, Monday's is EST
  CHECK(nextPowerballDraw(utc(2027, 11, 6, 12)) == utc(2027, 11, 7, 2, 59));
  CHECK(nextPowerballDraw(utc(2027, 11, 7, 12)) == utc(2027, 11, 9, 3, 59));
}

static Game mk(const char *id, const char *lg, char st, const char *away, int as, const char *home, int hs) {
  Game g;
  g.id = id;
  g.league = lg;
  g.state = st;
  g.away.abbr = away;
  g.away.score = as;
  g.home.abbr = home;
  g.home.score = hs;
  return g;
}

static void testPickem() {
  std::map<String, std::vector<Game>> games;
  games["nfl"] = {mk("1", "nfl", 'f', "MIA", 20, "BUF", 31), mk("2", "nfl", 'i', "KC", 7, "LV", 3),
                  mk("3", "nfl", 'f', "NE", 17, "NYJ", 17), mk("4", "nfl", 'f', "DAL", 10, "PHI", 9)};
  games["nba"] = {mk("1", "nba", 'f', "LAL", 100, "BOS", 90)};
  std::vector<PickemPick> p(7);
  p[0] = {"nfl", "1", "BUF", ' '};   // home won
  p[1] = {"nfl", "1", "MIA", ' '};   // away lost
  p[2] = {"nfl", "2", "KC", ' '};    // live: pending
  p[3] = {"nfl", "3", "NE", ' '};    // tie -> push
  p[4] = {"nba", "1", "LAL", ' '};   // same id, other league
  p[5] = {"nfl", "99", "SF", ' '};   // not on the board
  p[6] = {"mlb", "1", "LAD", ' '};   // league without games
  CHECK(pickemScore(p, games));
  CHECK(p[0].result == 'W' && p[1].result == 'L' && p[2].result == ' ' && p[3].result == 'T' &&
        p[4].result == 'W' && p[5].result == ' ' && p[6].result == ' ');
  CHECK(!pickemScore(p, games));   // nothing new
  games["nfl"][1].state = 'f';
  games["nfl"][1].home.score = 10;
  CHECK(pickemScore(p, games) && p[2].result == 'L');
  // a pick for a team that is not in the game is ignored
  std::vector<PickemPick> q{{"nfl", "4", "XXX", ' '}};
  CHECK(!pickemScore(q, games) && q[0].result == ' ');
  // a corrected final score flips the result
  games["nfl"][3].home.score = 12;
  std::vector<PickemPick> r{{"nfl", "4", "DAL", 'W'}};
  CHECK(pickemScore(r, games) && r[0].result == 'L');
  std::vector<PickemPick> none;
  CHECK(!pickemScore(none, games));
}

static StandRow row(const char *a, int w, int l, int seed = 0, const char *gb = "-", int t = 0) {
  StandRow r;
  r.abbr = a;
  r.wins = w;
  r.losses = l;
  r.ties = t;
  r.seed = seed;
  r.gb = gb;
  return r;
}

static void testPlayoff() {
  StandGroup g;
  g.league = "nfl";
  g.name = "AFC East";
  g.rows = {row("BUF", 10, 3, 2), row("MIA", 7, 6, 0, "3"), row("NYJ", 4, 9, 0, "6"), row("NE", 3, 10, 0, "7", 0)};
  PlayoffLine a = playoffLine(g, "BUF");
  CHECK(a.ok && a.rank == 1 && a.seed == 2 && a.gb == "-" && a.record == "10-3");
  CHECK(a.magic == 17 + 1 - 10 - 6);    // 2nd place has 6 losses
  PlayoffLine b = playoffLine(g, "NYJ");
  CHECK(b.ok && b.rank == 3 && b.gb == "6" && b.magic == -1 && b.record == "4-9");
  CHECK(!playoffLine(g, "DEN").ok);
  // clinched: clamp at 0
  g.rows[0].wins = 14;
  g.rows[1].losses = 12;
  CHECK(playoffLine(g, "BUF").magic == 0);
  // ties show in the record
  g.rows[0].ties = 1;
  CHECK(playoffLine(g, "BUF").record == "14-3-1");
  // other leagues
  StandGroup n;
  n.league = "nba";
  n.rows = {row("BOS", 40, 10), row("NYK", 35, 15, 0, "5")};
  CHECK(playoffLine(n, "BOS").magic == 82 + 1 - 40 - 15);
  n.league = "mlb";
  CHECK(playoffLine(n, "BOS").magic == 162 + 1 - 40 - 15);
  n.league = "nhl";
  CHECK(playoffLine(n, "BOS").magic == -1);
  n.league = "xyz";
  CHECK(playoffLine(n, "BOS").magic == -1);
  // a lone team or an empty group: no second place, no magic
  StandGroup one;
  one.league = "nfl";
  one.rows = {row("KC", 12, 1)};
  CHECK(playoffLine(one, "KC").ok && playoffLine(one, "KC").magic == -1);
  StandGroup empty;
  CHECK(!playoffLine(empty, "KC").ok);
}

int main() {
  testLaunches();
  testWaits();
  testStocks();
  testNews();
  testDrive();
  testLotto();
  testPickem();
  testPlayoff();
  if (fails) {
    printf("%d FAILED\n", fails);
    return 1;
  }
  printf("all batch 5 feed tests passed\n");
  return 0;
}
