// feeds5.cpp - batch 5 data feeds. Fetchers filter the JSON down while streaming (ESP32 RAM is small); the
// parse* functions are pure and covered by test/native/test_batch5.cpp against saved responses.
#include "feeds5.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <set>
#include "desk.h"
#include "espn.h"
#include "net.h"

// ---------------------------------------------------------------- small helpers
static String upperS(String s) {
  s.toUpperCase();
  return s;
}
static String lowerS(String s) {
  s.toLowerCase();
  return s;
}
// numbers arrive as JSON numbers or as strings ("353.92")
static float numOf(JsonVariantConst v) {
  if (v.is<float>() || v.is<int>()) return v.as<float>();
  const char *s = v.as<const char *>();
  return s ? (float)atof(s) : 0.0f;
}
static int intOf(JsonVariantConst v) { return (int)lroundf(numOf(v)); }

// days since 1970-01-01 <-> civil date
static long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}
static int yearOfDays(long z) {
  z += 719468;
  const long era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned m = mp < 10 ? mp + 3 : mp - 9;
  return (int)(yoe + era * 400) + (m <= 2);
}
static long floorDiv(long a, long b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
static int weekdayOfDays(long days) { return (int)(((days % 7) + 7 + 4) % 7); }  // 0 = Sunday

// US Eastern time with the fixed rule in force since 2007: DST from the 2nd Sunday of March 02:00 EST (07:00 UTC)
// to the 1st Sunday of November 02:00 EDT (06:00 UTC). Lets us avoid touching the TZ the rest of the clock uses.
static int etOffset(time_t utc) {
  int y = yearOfDays(floorDiv((long)utc - 5 * 3600, 86400));
  long m1 = daysFromCivil(y, 3, 1);
  long start = m1 + (7 - weekdayOfDays(m1)) % 7 + 7;
  long n1 = daysFromCivil(y, 11, 1);
  long end = n1 + (7 - weekdayOfDays(n1)) % 7;
  long s = start * 86400L + 7 * 3600, e = end * 86400L + 6 * 3600;
  return ((long)utc >= s && (long)utc < e) ? -4 * 3600 : -5 * 3600;
}
static long etDay(time_t utc) { return floorDiv((long)utc + etOffset(utc), 86400); }

// ---------------------------------------------------------------- launches
static bool launchDone(const String &st) { return st == "Success" || st == "Failure" || st == "Partial Failure"; }

void parseLaunches(const JsonDocument &d, std::vector<Launch> &out) {
  out.clear();
  for (JsonObjectConst r : d["results"].as<JsonArrayConst>()) {
    Launch l;
    l.id = r["id"] | "";
    l.name = foldAscii(String(r["name"] | ""));
    l.status = r["status"]["abbrev"] | "";
    l.net = parseIsoUtc(String(r["net"] | ""));
    if (!l.id.length() && !l.name.length()) continue;
    if (launchDone(l.status)) continue;  // already flown (the "upcoming" list still holds today's finished ones)
    // names look like "Falcon 9 Block 5 | USSF-266"
    int bar = l.name.indexOf(" | ");
    l.rocket = r["rocket"]["configuration"]["name"] | "";
    if (!l.rocket.length()) l.rocket = bar > 0 ? l.name.substring(0, bar) : l.name;
    l.mission = foldAscii(String(r["mission"]["name"] | ""));
    if (!l.mission.length() && bar > 0) l.mission = l.name.substring(bar + 3);
    String padName = r["pad"]["name"] | "";
    String loc = r["pad"]["location"]["name"] | "";
    l.pad = (padName.length() && padName != "Unknown Pad") ? padName : loc;
    l.provider = r["launch_service_provider"]["abbrev"] | (r["launch_service_provider"]["name"] | "");
    String lw = lowerS(loc + " " + padName);
    l.vandenberg = lw.indexOf("vandenberg") >= 0;
    out.push_back(l);
  }
  std::stable_sort(out.begin(), out.end(), [](const Launch &a, const Launch &b) { return a.net < b.net; });
}

bool fetchLaunches(std::vector<Launch> &out) {
  JsonDocument filter;
  JsonObject r = filter["results"][0].to<JsonObject>();
  r["id"] = true;
  r["name"] = true;
  r["net"] = true;
  r["status"]["abbrev"] = true;
  r["rocket"]["configuration"]["name"] = true;
  r["mission"]["name"] = true;
  r["pad"]["name"] = true;
  r["pad"]["location"]["name"] = true;
  r["launch_service_provider"]["abbrev"] = true;
  r["launch_service_provider"]["name"] = true;
  JsonDocument d;
  // normal mode (not mode=list): list mode leaves out rocket, mission, pad and provider. The raw reply is ~60 KB,
  // the filter keeps it to ~2 KB. A few extra rows cover today's already-flown launches.
  int code = httpGetJson("https://ll.thespacedevs.com/2.3.0/launches/upcoming/?limit=8", d, &filter);
  if (code != 200) {
    Serial.printf("[launch] failed: %d\n", code);
    return false;
  }
  parseLaunches(d, out);
  if (out.size() > 6) out.resize(6);
  return true;
}

// ---------------------------------------------------------------- park waits
static void addRides(JsonArrayConst rides, std::set<long> &seen, std::vector<RideWait> &out) {
  for (JsonObjectConst r : rides) {
    long id = r["id"] | -1L;
    if (id >= 0 && !seen.insert(id).second) continue;  // same ride listed under its land and at the top level
    if (!(r["is_open"] | false)) continue;
    RideWait w;
    w.name = foldAscii(String(r["name"] | ""));
    w.wait = intOf(r["wait_time"]);
    w.open = true;
    if (w.name.length()) out.push_back(w);
  }
}

void parseParkWaits(const JsonDocument &d, ParkWaits &out) {
  out.rides.clear();
  out.ok = false;
  if (d["lands"].isNull() && d["rides"].isNull()) return;
  std::set<long> seen;
  for (JsonObjectConst land : d["lands"].as<JsonArrayConst>()) addRides(land["rides"], seen, out.rides);
  addRides(d["rides"], seen, out.rides);
  std::stable_sort(out.rides.begin(), out.rides.end(),
                   [](const RideWait &a, const RideWait &b) { return a.wait > b.wait; });
  out.ok = true;
}

bool fetchParkWaits(const String &parkId, const String &parkName, ParkWaits &out) {
  JsonDocument filter;
  JsonObject lr = filter["lands"][0]["rides"][0].to<JsonObject>();
  lr["id"] = true;
  lr["name"] = true;
  lr["is_open"] = true;
  lr["wait_time"] = true;
  JsonObject tr = filter["rides"][0].to<JsonObject>();
  tr["id"] = true;
  tr["name"] = true;
  tr["is_open"] = true;
  tr["wait_time"] = true;
  JsonDocument d;
  int code = httpGetJson(String("https://queue-times.com/parks/") + parkId + "/queue_times.json", d, &filter);
  if (code != 200) {
    Serial.printf("[waits] failed: %d\n", code);
    return false;
  }
  parseParkWaits(d, out);
  out.park = parkName;
  out.fetched = time(nullptr);
  return out.ok;
}

// ---------------------------------------------------------------- stocks
void stockRollover(StockQuote &q, time_t now) {
  if (q.at == 0 || etDay(q.at) == etDay(now)) return;
  if (q.price > 0) q.prevClose = q.price;
  q.open = 0;
  q.spark.clear();
}

bool parseStock(const JsonDocument &d, StockQuote &q, time_t now) {
  JsonObjectConst r = d["quotes"][0];
  if (r.isNull()) return false;
  float bid = numOf(r["bid"]), ask = numOf(r["ask"]);
  if (bid <= 0 && ask <= 0) return false;
  float price = (bid > 0 && ask > 0) ? (bid + ask) / 2 : (bid > 0 ? bid : ask);
  stockRollover(q, now);
  q.bid = bid;
  q.ask = ask;
  q.price = price;
  q.dayHigh = numOf(r["dailyHigh"]);
  q.dayLow = numOf(r["dailyLow"]);
  q.halted = r["isTradingHalt"] | false;
  if (q.open <= 0) q.open = price;
  q.at = now;   // when the clock saw it (generatedAt can sit on Friday all weekend, which would break the rollover)
  q.ok = true;
  // at most 64 samples: when full keep every other one (plus the newest) so the whole day's shape survives
  if (q.spark.size() >= 64) {
    std::vector<float> thin;
    for (size_t i = 0; i < q.spark.size(); i += 2) thin.push_back(q.spark[i]);
    if ((q.spark.size() - 1) % 2 != 0) thin.push_back(q.spark.back());
    q.spark.swap(thin);
  }
  q.spark.push_back(price);
  return true;
}

bool fetchStock(const String &symbol, StockQuote &q, time_t now) {
  JsonDocument filter;
  JsonObject f = filter["quotes"][0].to<JsonObject>();
  f["bid"] = true;
  f["ask"] = true;
  f["dailyHigh"] = true;
  f["dailyLow"] = true;
  f["isTradingHalt"] = true;
  f["generatedAt"] = true;
  JsonDocument d;
  int code = httpGetJson(String("https://api.robinhood.com/rhj/prices/") + symbol, d, &filter);
  if (code != 200) {
    Serial.printf("[stock] %s failed: %d\n", symbol.c_str(), code);
    return false;
  }
  q.symbol = symbol;
  return parseStock(d, q, now);
}

void parseStockList(const JsonDocument &d, std::vector<std::pair<String, String>> &out) {
  out.clear();
  for (JsonObjectConst a : d["assets"].as<JsonArrayConst>()) {
    String sym = a["tokenSymbol"] | "";
    if (!sym.length()) continue;
    String name = a["tokenName"] | "";   // "Apple Inc. â¢ Robinhood Token"
    int dot = name.indexOf(" â¢");
    if (dot < 0) dot = name.indexOf(" Robinhood Token");
    if (dot > 0) name = name.substring(0, dot);
    name = foldAscii(name);
    name.trim();
    // the reply is ~170 KB of which these two strings are all we keep
    out.push_back({sym, name});
  }
  std::sort(out.begin(), out.end(), [](const std::pair<String, String> &a, const std::pair<String, String> &b) {
    return a.first < b.first;
  });
  out.erase(std::unique(out.begin(), out.end(),
                        [](const std::pair<String, String> &a, const std::pair<String, String> &b) {
                          return a.first == b.first;
                        }),
            out.end());
}

bool fetchStockList(std::vector<std::pair<String, String>> &out) {
  JsonDocument filter;
  filter["assets"][0]["tokenSymbol"] = true;
  filter["assets"][0]["tokenName"] = true;
  JsonDocument d;
  int code = httpGetJson("https://api.robinhood.com/rhj/assets", d, &filter);
  if (code != 200) {
    Serial.printf("[stock] list failed: %d\n", code);
    return false;
  }
  parseStockList(d, out);
  return !out.empty();
}

// ---------------------------------------------------------------- news
// "Josh Allen" -> "J.ALLEN"
static String shortPlayer(const String &name) {
  int sp = name.lastIndexOf(' ');
  if (sp <= 0) return upperS(name);
  return upperS(name.substring(0, 1) + "." + name.substring(sp + 1));
}

void parseNews(const JsonDocument &d, const String &league, const NewsWatch &w, std::vector<NewsItem> &out) {
  out.clear();
  std::set<String> teamSet;
  for (const auto &t : w.teams)
    if (t.first == league) teamSet.insert(upperS(t.second));
  std::set<String> idSet(w.athleteIds.begin(), w.athleteIds.end());
  for (JsonObjectConst a : d["articles"].as<JsonArrayConst>()) {
    NewsItem n;
    // id: ESPN's own article id, else the data source id
    if (a["id"].is<const char *>()) n.id = a["id"].as<const char *>();
    else if (!a["id"].isNull()) n.id = String(a["id"].as<long>());
    if (!n.id.length()) n.id = a["dataSourceIdentifier"] | "";
    n.headline = foldAscii(String(a["headline"] | ""));
    if (!n.id.length() || !n.headline.length()) continue;
    n.league = league;
    n.published = parseIsoUtc(String(a["published"] | ""));
    String teamHit, playerHit;
    for (JsonObjectConst c : a["categories"].as<JsonArrayConst>()) {
      String type = c["type"] | "";
      if (type == "team" && !teamHit.length()) {
        // ESPN tags carry the team abbreviation (and the numeric team id, which we don't track), so match on that
        String ab = upperS(String(c["team"]["abbreviation"] | ""));
        if (ab.length() && teamSet.count(ab)) teamHit = ab;
      } else if (type == "athlete" && !playerHit.length()) {
        String aid;
        if (c["athleteId"].is<const char *>()) aid = c["athleteId"].as<const char *>();
        else if (!c["athleteId"].isNull()) aid = String(c["athleteId"].as<long>());
        String nm = c["description"] | "";
        bool hit = aid.length() && idSet.count(aid);
        for (const String &p : w.playerNames)
          if (nm.length() && lowerS(nm) == lowerS(p)) hit = true;
        if (hit) playerHit = shortPlayer(nm.length() ? nm : aid);
      }
    }
    String hl = lowerS(n.headline);
    if (!playerHit.length())
      for (const String &p : w.playerNames)
        if (p.length() && hl.indexOf(lowerS(p)) >= 0) {
          playerHit = shortPlayer(p);
          break;
        }
    if (!teamHit.length() && !playerHit.length()) continue;
    // a keyword in the headline is the more useful label ("TRADE" over "BUF")
    String why = playerHit.length() ? playerHit : teamHit;
    for (const String &k : w.keywords)
      if (k.length() && hl.indexOf(lowerS(k)) >= 0) {
        why = upperS(k);
        break;
      }
    n.why = why;
    out.push_back(n);
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const NewsItem &a, const NewsItem &b) { return a.published > b.published; });
}

bool fetchNews(const String &league, const NewsWatch &w, std::vector<NewsItem> &out) {
  JsonDocument filter;
  JsonObject a = filter["articles"][0].to<JsonObject>();
  a["id"] = true;
  a["dataSourceIdentifier"] = true;
  a["headline"] = true;
  a["published"] = true;
  JsonObject c = a["categories"][0].to<JsonObject>();
  c["type"] = true;
  c["description"] = true;
  c["athleteId"] = true;
  c["team"]["abbreviation"] = true;
  JsonDocument d;
  String url = String("https://site.api.espn.com/apis/site/v2/sports/") + espnSport(league) + "/" + league +
               "/news?limit=40";
  int code = httpGetJson(url, d, &filter);
  if (code != 200) {
    Serial.printf("[news] %s failed: %d\n", league.c_str(), code);
    return false;
  }
  parseNews(d, league, w, out);
  return true;
}

// ---------------------------------------------------------------- TomTom
void parseDriveTime(const JsonDocument &d, DriveTime &out) {
  JsonObjectConst s = d["routes"][0]["summary"];
  out.ok = false;
  if (s.isNull()) return;
  int secs = intOf(s["travelTimeInSeconds"]);
  if (secs <= 0) return;
  int free = intOf(s["noTrafficTravelTimeInSeconds"]);
  int delay = intOf(s["trafficDelayInSeconds"]);
  out.minutes = (secs + 30) / 60;
  out.typical = free > 0 ? (free + 30) / 60 : out.minutes;
  out.delay = (delay + 30) / 60;
  out.km = numOf(s["lengthInMeters"]) / 1000.0f;
  out.ok = true;
}

static String pctEncode(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
    else {
      char b[4];
      snprintf(b, sizeof b, "%%%02X", c);
      o += b;
    }
  }
  return o;
}

bool fetchDriveTime(const String &key, float fromLat, float fromLon, float toLat, float toLon, DriveTime &out) {
  if (!key.length()) return false;
  JsonDocument filter;
  JsonObject s = filter["routes"][0]["summary"].to<JsonObject>();
  s["lengthInMeters"] = true;
  s["travelTimeInSeconds"] = true;
  s["trafficDelayInSeconds"] = true;
  s["noTrafficTravelTimeInSeconds"] = true;
  char pos[80];
  snprintf(pos, sizeof pos, "%.5f,%.5f:%.5f,%.5f", fromLat, fromLon, toLat, toLon);
  JsonDocument d;
  int code = httpGetJson(String("https://api.tomtom.com/routing/1/calculateRoute/") + pos +
                             "/json?traffic=true&key=" + key, d, &filter);
  if (code != 200) {
    Serial.printf("[drive] failed: %d\n", code);   // never print the URL: it holds the key
    return false;
  }
  parseDriveTime(d, out);
  if (out.ok) out.at = time(nullptr);
  return out.ok;
}

bool parseGeocode(const JsonDocument &d, float &lat, float &lon) {
  JsonObjectConst p = d["results"][0]["position"];
  if (p.isNull() || p["lat"].isNull() || p["lon"].isNull()) return false;
  lat = numOf(p["lat"]);
  lon = numOf(p["lon"]);
  return true;
}

bool geocodeTomTom(const String &key, const String &address, float &lat, float &lon) {
  if (!key.length() || !address.length()) return false;
  JsonDocument filter;
  filter["results"][0]["position"]["lat"] = true;
  filter["results"][0]["position"]["lon"] = true;
  JsonDocument d;
  int code = httpGetJson(String("https://api.tomtom.com/search/2/geocode/") + pctEncode(address) +
                             ".json?limit=1&key=" + key, d, &filter);
  if (code != 200) {
    Serial.printf("[drive] geocode failed: %d\n", code);
    return false;
  }
  return parseGeocode(d, lat, lon);
}

// ---------------------------------------------------------------- Powerball
time_t nextPowerballDraw(time_t now) {
  long d0 = etDay(now);
  for (int k = 0; k < 8; k++) {
    long day = d0 + k;
    int wd = weekdayOfDays(day);
    if (wd != 1 && wd != 3 && wd != 6) continue;
    // the offset in force at noon that day also holds at 22:59 (DST changes at 02:00)
    int off = etOffset((time_t)(day * 86400L + 17 * 3600));
    time_t t = (time_t)(day * 86400L + 22 * 3600 + 59 * 60 - off);
    if (t > now) return t;
  }
  return 0;
}

void parseLotto(const JsonDocument &d, LottoDraw &out) {
  out = LottoDraw();
  JsonObjectConst r = d.is<JsonArrayConst>() ? d[0] : d["draws"][0];
  if (r.isNull()) return;
  out.drawDate = parseIsoUtc(String(r["draw_date"] | ""));
  const char *s = r["winning_numbers"] | "";
  int nums[8], n = 0;
  while (*s && n < 8) {
    while (*s == ' ') s++;
    if (!isdigit((unsigned char)*s)) break;
    nums[n++] = atoi(s);
    while (isdigit((unsigned char)*s)) s++;
  }
  if (n < 6 || !out.drawDate) return;
  for (int i = 0; i < 5; i++) out.white[i] = nums[i];
  out.power = nums[5];
  out.multiplier = intOf(r["multiplier"]);
  out.ok = true;
}

bool fetchLotto(LottoDraw &out, time_t now) {
  // No free, key-less, reliable JSON source for the jackpot exists (powerball.com's estimates API redirects to
  // an HTML page, state lotteries only publish it inside web pages), so the jackpot stays "".
  JsonDocument filter;
  filter[0]["draw_date"] = true;
  filter[0]["winning_numbers"] = true;
  filter[0]["multiplier"] = true;
  JsonDocument d;
  int code = httpGetJson("https://data.ny.gov/resource/d6yy-54nr.json?$order=draw_date%20DESC&$limit=1", d, &filter);
  if (code != 200) {
    Serial.printf("[lotto] failed: %d\n", code);
    return false;
  }
  parseLotto(d, out);
  out.nextDraw = nextPowerballDraw(now);
  return out.ok;
}

// ---------------------------------------------------------------- pick'em
bool pickemScore(std::vector<PickemPick> &picks, const std::map<String, std::vector<Game>> &games) {
  bool changed = false;
  for (PickemPick &p : picks) {
    auto it = games.find(p.league);
    if (it == games.end()) continue;
    for (const Game &g : it->second) {
      if (g.id != p.gameId) continue;
      if (g.state != 'f') break;
      int mine, theirs;
      if (g.away.abbr == p.pick) mine = g.away.score, theirs = g.home.score;
      else if (g.home.abbr == p.pick) mine = g.home.score, theirs = g.away.score;
      else break;
      char r = mine > theirs ? 'W' : (mine < theirs ? 'L' : 'T');
      if (r != p.result) {
        p.result = r;
        changed = true;
      }
      break;
    }
  }
  return changed;
}

// ---------------------------------------------------------------- playoff race
PlayoffLine playoffLine(const StandGroup &g, const String &team) {
  PlayoffLine pl;
  for (size_t i = 0; i < g.rows.size(); i++) {
    const StandRow &r = g.rows[i];
    if (r.abbr != team) continue;
    pl.ok = true;
    pl.rank = (int)i + 1;
    pl.seed = r.seed;
    pl.gb = r.gb;
    pl.record = String(r.wins) + "-" + String(r.losses);
    if (r.ties > 0) pl.record += String("-") + String(r.ties);
    int games = g.league == "nfl" ? 17 : g.league == "nba" ? 82 : g.league == "mlb" ? 162 : -1;  // nhl: points race
    if (i == 0 && games > 0 && g.rows.size() >= 2) {
      pl.magic = std::max(0, games + 1 - r.wins - g.rows[1].losses);
    }
    return pl;
  }
  return pl;
}
