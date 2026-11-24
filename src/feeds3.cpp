#include "feeds3.h"
#include <algorithm>
#include <cmath>
#include "espn.h"
#include "net.h"

static String upper(String s) {
  s.toUpperCase();
  return s;
}

// ---------------------------------------------------------------- win probability + top performers
void parseWinProb(const JsonDocument &d, const Game &g, WinProb &out) {
  out.gameKey = g.key();
  out.game = g;
  out.home.clear();
  for (JsonObjectConst w : d["winprobability"].as<JsonArrayConst>()) {
    float h = w["homeWinPercentage"] | -1.0f;
    if (h >= 0) out.home.push_back((uint8_t)lroundf(std::min(1.0f, h) * 100));
  }
  out.ok = out.home.size() >= 2;
}

// "27/45, 304 YDS, 1 TD, 2 INT" -> "304 YDS 1 TD"; NBA's bare "27" gets its label
static String shortLine(const String &cat, const String &v) {
  if (cat == "points") return v + " PTS";
  if (cat == "rebounds") return v + " REB";
  if (cat == "assists") return v + " AST";
  String out;
  int start = 0;
  while (start < (int)v.length()) {
    int comma = v.indexOf(',', start);
    String part = v.substring(start, comma < 0 ? v.length() : comma);
    part.trim();
    start = comma < 0 ? v.length() : comma + 1;
    if (part.indexOf('/') >= 0 || part.endsWith(" CAR") || part.endsWith(" REC") || part.indexOf(" INT") > 0) continue;
    if (out.length()) out += ' ';
    out += part;
  }
  return out.length() ? out : v;
}
static int leadingInt(const String &s) {
  for (int i = 0; i < (int)s.length(); i++)
    if (isdigit((unsigned char)s[i])) return atoi(s.c_str() + i);
  return 0;
}
// yards out of "22 CAR, 89 YDS, 1 TD"
static int yardsOf(const String &v) {
  int y = v.indexOf(" YDS");
  if (y < 0) return 0;
  int s = v.lastIndexOf(' ', y - 1);
  return atoi(v.c_str() + s + 1);
}

void parseHighlights(const JsonDocument &d, const Game &g, std::vector<Highlight> &out) {
  out.clear();
  for (JsonObjectConst t : d["leaders"].as<JsonArrayConst>()) {
    String abbr = t["team"]["abbreviation"] | "";
    const Team &tm = g.home.abbr == abbr ? g.home : g.away;
    String bestCat, bestWho, bestVal;
    int bestScore = -1;
    for (JsonObjectConst l : t["leaders"].as<JsonArrayConst>()) {
      String cat = l["name"] | "";
      JsonObjectConst top = l["leaders"][0];
      if (top.isNull()) continue;
      String who = top["athlete"]["shortName"] | (top["athlete"]["displayName"] | "");
      String val = top["displayValue"] | "";
      // the most telling line: NFL yards (passing counts half, it is always the biggest number), NBA points
      int score;
      if (cat == "passingYards") score = yardsOf(val) / 2 + 20;
      else if (cat == "rushingYards" || cat == "receivingYards") score = yardsOf(val);
      else if (cat == "points") score = 1000 + leadingInt(val);
      else if (cat == "rebounds" || cat == "assists") score = 500 + leadingInt(val);
      else if (g.league == "mlb") score = 10;  // baseball: the first category ESPN lists
      else continue;
      if (score > bestScore) bestScore = score, bestCat = cat, bestWho = who, bestVal = val;
    }
    if (bestScore < 0) continue;
    Highlight h;
    h.gameKey = g.key();
    h.team = abbr;
    h.who = upper(bestWho);
    h.what = upper(shortLine(bestCat, bestVal));
    h.color = tm.color;
    if (tm.abbr == abbr) {
      uint32_t c = tm.color, a = tm.alt;
      auto lum = [](uint32_t x) { return (((x >> 16) & 255) * 3 + ((x >> 8) & 255) * 6 + (x & 255)) / 10; };
      h.color = lum(c) < 40 ? (lum(a) < 40 ? 0xA5ACAF : a) : c;
    }
    out.push_back(h);
  }
}

bool fetchGameExtras(const Game &g, bool wantLeaders, WinProb &wp, std::vector<Highlight> &hl) {
  JsonDocument filter;
  filter["winprobability"][0]["homeWinPercentage"] = true;
  if (wantLeaders) {
    JsonObject l = filter["leaders"][0].to<JsonObject>();
    l["team"]["abbreviation"] = true;
    JsonObject c = l["leaders"][0].to<JsonObject>();
    c["name"] = true;
    c["leaders"][0]["displayValue"] = true;
    c["leaders"][0]["athlete"]["shortName"] = true;
    c["leaders"][0]["athlete"]["displayName"] = true;
  }
  JsonDocument d;
  String url = String("https://site.api.espn.com/apis/site/v2/sports/") + espnSport(g.league) + "/" + g.league +
               "/summary?event=" + g.id;
  if (httpGetJson(url, d, &filter) != 200) return false;
  parseWinProb(d, g, wp);
  if (wantLeaders) parseHighlights(d, g, hl);
  return true;
}

// ---------------------------------------------------------------- golf
void parseGolf(const JsonDocument &d, GolfBoard &out) {
  out = GolfBoard();
  JsonObjectConst e = d["events"][0];
  if (e.isNull()) return;
  out.event = upper(e["shortName"] | (e["name"] | ""));
  out.start = parseIsoUtc(e["date"] | "");
  JsonObjectConst c = e["competitions"][0];
  const char *st = c["status"]["type"]["state"] | (e["status"]["type"]["state"] | "pre");
  out.state = !strcmp(st, "in") ? 'i' : !strcmp(st, "post") ? 'f' : 'p';
  out.detail = c["status"]["type"]["detail"] | (e["status"]["type"]["detail"] | "");
  struct R {
    int order;
    String name, score;
  };
  std::vector<R> rows;
  for (JsonObjectConst p : c["competitors"].as<JsonArrayConst>()) {
    R r;
    r.order = p["order"] | 999;
    r.name = upper(p["athlete"]["shortName"] | (p["athlete"]["displayName"] | ""));
    r.score = p["score"].is<const char *>() ? String(p["score"].as<const char *>()) : String(p["score"]["displayValue"] | "");
    if (r.name.length()) rows.push_back(r);
  }
  std::stable_sort(rows.begin(), rows.end(), [](const R &a, const R &b) { return a.order < b.order; });
  if (rows.size() > 10) rows.resize(10);
  // positions with ties: "1", "T2", "T2", "4"
  for (size_t i = 0; i < rows.size(); i++) {
    size_t first = i;
    while (first > 0 && rows[first - 1].score == rows[i].score) first--;
    bool tie = (first < i) || (i + 1 < rows.size() && rows[i + 1].score == rows[i].score);
    GolfRow g;
    g.pos = (tie ? "T" : "") + String((int)first + 1);
    g.name = rows[i].name;
    g.score = rows[i].score == "0" ? "E" : rows[i].score;
    out.rows.push_back(g);
  }
  out.ok = !out.rows.empty();
}

bool fetchGolf(const String &tour, GolfBoard &out) {
  JsonDocument filter;
  JsonObject e = filter["events"][0].to<JsonObject>();
  e["name"] = true;
  e["shortName"] = true;
  e["date"] = true;
  e["status"]["type"]["state"] = true;
  e["status"]["type"]["detail"] = true;
  JsonObject c = e["competitions"][0].to<JsonObject>();
  c["status"]["type"]["state"] = true;
  c["status"]["type"]["detail"] = true;
  JsonObject p = c["competitors"][0].to<JsonObject>();
  p["order"] = true;
  p["score"] = true;
  p["athlete"]["shortName"] = true;
  p["athlete"]["displayName"] = true;
  JsonDocument d;
  if (httpGetJson("https://site.api.espn.com/apis/site/v2/sports/golf/" + tour + "/scoreboard", d, &filter) != 200)
    return false;
  parseGolf(d, out);
  return true;
}

// ---------------------------------------------------------------- next game
bool parseNextGame(const JsonDocument &d, const String &league, const String &team, time_t now, NextGame &out) {
  out = NextGame();
  for (JsonObjectConst e : d["events"].as<JsonArrayConst>()) {
    time_t start = parseIsoUtc(e["date"] | "");
    JsonObjectConst c = e["competitions"][0];
    const char *st = c["status"]["type"]["state"] | "pre";
    if (strcmp(st, "pre") || start + 4 * 3600 < now) continue;
    out.league = league;
    out.id = e["id"] | "";
    out.start = start;
    for (JsonObjectConst t : c["competitors"].as<JsonArrayConst>()) {
      String ab = t["team"]["abbreviation"] | "";
      bool home = !strcmp(t["homeAway"] | "", "home");
      if (ab.equalsIgnoreCase(team)) out.us = ab, out.home = home;
      else out.them = ab;
    }
    for (JsonObjectConst b : c["broadcasts"].as<JsonArrayConst>()) {
      String n = b["media"]["shortName"] | "";
      if (n.length()) {
        out.tv = upper(n);
        break;
      }
    }
    out.ok = out.us.length() && out.them.length();
    return out.ok;
  }
  return false;
}

bool fetchNextGame(const String &league, const String &team, time_t now, NextGame &out) {
  JsonDocument filter;
  JsonObject e = filter["events"][0].to<JsonObject>();
  e["id"] = true;
  e["date"] = true;
  JsonObject c = e["competitions"][0].to<JsonObject>();
  c["status"]["type"]["state"] = true;
  c["competitors"][0]["homeAway"] = true;
  c["competitors"][0]["team"]["abbreviation"] = true;
  c["broadcasts"][0]["media"]["shortName"] = true;
  String t = team;
  t.toLowerCase();
  JsonDocument d;
  String url = String("https://site.api.espn.com/apis/site/v2/sports/") + espnSport(league) + "/" + league + "/teams/" +
               t + "/schedule";
  if (httpGetJson(url, d, &filter) != 200) return false;
  parseNextGame(d, league, team, now, out);
  return true;  // the download worked, even if the season is over
}

// ---------------------------------------------------------------- planes overhead (OpenSky Network)
static const float DEG = 3.14159265f / 180;
float distanceKm(float lat1, float lon1, float lat2, float lon2) {
  float dLat = (lat2 - lat1) * DEG, dLon = (lon2 - lon1) * DEG;
  float a = sinf(dLat / 2) * sinf(dLat / 2) + cosf(lat1 * DEG) * cosf(lat2 * DEG) * sinf(dLon / 2) * sinf(dLon / 2);
  return 6371.0f * 2 * atan2f(sqrtf(a), sqrtf(1 - a));
}
float bearingDeg(float lat1, float lon1, float lat2, float lon2) {
  float y = sinf((lon2 - lon1) * DEG) * cosf(lat2 * DEG);
  float x = cosf(lat1 * DEG) * sinf(lat2 * DEG) - sinf(lat1 * DEG) * cosf(lat2 * DEG) * cosf((lon2 - lon1) * DEG);
  float b = atan2f(y, x) / DEG;
  return b < 0 ? b + 360 : b;
}

void parseFlights(const JsonDocument &d, float lat, float lon, int radiusKm, std::vector<Flight> &out) {
  out.clear();
  // each state: [icao24, callsign, country, t_pos, t_contact, lon, lat, baro_alt_m, on_ground, velocity_ms,
  //              true_track, vertical_rate_ms, sensors, geo_alt_m, squawk, spi, source]
  for (JsonArrayConst s : d["states"].as<JsonArrayConst>()) {
    if (s[5].isNull() || s[6].isNull() || (s[8] | false)) continue;
    Flight f;
    f.callsign = s[1] | "";
    f.callsign.trim();
    if (!f.callsign.length()) f.callsign = upper(s[0] | "?");
    float flat = s[6].as<float>(), flon = s[5].as<float>();
    f.distKm = distanceKm(lat, lon, flat, flon);
    if (f.distKm > radiusKm) continue;
    f.bearing = bearingDeg(lat, lon, flat, flon);
    float alt = s[7].isNull() ? (s[13] | 0.0f) : s[7].as<float>();
    f.altFt = (int)lroundf(alt * 3.28084f);
    f.speedKt = (int)lroundf((s[9] | 0.0f) * 1.94384f);
    f.heading = (int)lroundf(s[10] | 0.0f) % 360;
    f.climbFpm = (int)lroundf((s[11] | 0.0f) * 196.85f);
    out.push_back(f);
  }
  std::sort(out.begin(), out.end(), [](const Flight &a, const Flight &b) { return a.distKm < b.distKm; });
  if (out.size() > 12) out.resize(12);
}

bool fetchFlights(float lat, float lon, int radiusKm, std::vector<Flight> &out) {
  float dLat = radiusKm / 111.0f, dLon = radiusKm / (111.0f * std::max(0.2f, cosf(lat * DEG)));
  char url[200];
  snprintf(url, sizeof url, "https://opensky-network.org/api/states/all?lamin=%.3f&lomin=%.3f&lamax=%.3f&lomax=%.3f",
           lat - dLat, lon - dLon, lat + dLat, lon + dLon);
  JsonDocument d;
  int code = httpGetJson(url, d);
  if (code != 200) {
    Serial.printf("[flights] opensky %d\n", code);
    return false;
  }
  parseFlights(d, lat, lon, radiusKm, out);
  return true;
}
