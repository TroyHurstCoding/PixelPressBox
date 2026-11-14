#include "feeds2.h"
#include "net.h"
#include "espn.h"
#include <cmath>

String urlDecode(const String &s) {
  String o;
  o.reserve(s.length());
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '%' && i + 2 < s.length()) {
      char h[3] = {s[i + 1], s[i + 2], 0};
      o += (char)strtol(h, nullptr, 16);
      i += 2;
    } else if (c == '+') {
      o += ' ';
    } else {
      o += c;
    }
  }
  return o;
}

String chartDateFor(time_t now, int yearsAgo) {
  // Hot 100 charts are dated Saturdays. Take the same calendar day N years back, then step back to its Saturday.
  struct tm t;
  localtime_r(&now, &t);
  struct tm b = {};
  b.tm_year = t.tm_year - yearsAgo;
  b.tm_mon = t.tm_mon;
  b.tm_mday = t.tm_mday == 29 && t.tm_mon == 1 ? 28 : t.tm_mday;
  b.tm_hour = 12;
  time_t x = mktime(&b);
  localtime_r(&x, &b);
  x -= (time_t)((b.tm_wday + 1) % 7) * 86400;  // Saturday = 6 -> back 0 days; Sunday -> back 1 ...
  localtime_r(&x, &b);
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", b.tm_year + 1900, b.tm_mon + 1, b.tm_mday);
  return buf;
}

bool fetchTrivia(std::vector<TriviaQ> &out) {
  JsonDocument d;
  if (httpGetJson("https://opentdb.com/api.php?amount=10&type=multiple&encode=url3986", d) != 200) return false;
  if ((d["response_code"] | 1) != 0) return false;
  out.clear();
  for (JsonObjectConst r : d["results"].as<JsonArrayConst>()) {
    TriviaQ q;
    q.category = urlDecode(r["category"] | "");
    int colon = q.category.indexOf(": ");  // "Science: Computers" -> "Computers"
    if (colon > 0) q.category = q.category.substring(colon + 2);
    q.question = urlDecode(r["question"] | "");
    q.answer = urlDecode(r["correct_answer"] | "");
    if (q.question.length() && q.answer.length()) out.push_back(q);
  }
  return !out.empty();
}

bool fetchFact(FactQuote &out) {
  JsonDocument d;
  if (httpGetJson("https://uselessfacts.jsph.pl/api/v2/facts/random?language=en", d) != 200) return false;
  out.text = d["text"] | "";
  out.text.replace("`", "'");
  out.author = "";
  return out.text.length() > 0;
}

bool fetchQuote(FactQuote &out) {
  JsonDocument d;
  if (httpGetJson("https://zenquotes.io/api/random", d) != 200) return false;
  out.text = d[0]["q"] | "";
  out.author = d[0]["a"] | "";
  if (out.text.startsWith("Too many requests")) return false;  // their rate-limit message
  return out.text.length() > 0;
}

bool fetchChart(time_t now, int yearsAgo, ChartWeek &out) {
  String date = chartDateFor(now, yearsAgo);
  JsonDocument filter;
  filter["date"] = true;
  filter["data"][0]["song"] = true;
  filter["data"][0]["artist"] = true;
  filter["data"][0]["this_week"] = true;
  JsonDocument d;
  String url = "https://raw.githubusercontent.com/mhollingshead/billboard-hot-100/main/date/" + date + ".json";
  if (httpGetJson(url, d, &filter) != 200) return false;
  out.top.clear();
  for (JsonObjectConst e : d["data"].as<JsonArrayConst>()) {
    int pos = e["this_week"] | 0;
    if (pos < 1 || pos > 3) continue;
    ChartEntry c;
    c.song = e["song"] | "";
    c.artist = e["artist"] | "";
    if ((int)out.top.size() < pos) out.top.resize(pos);
    out.top[pos - 1] = c;
  }
  out.ok = !out.top.empty() && out.top[0].song.length();
  out.date = date;
  out.yearsAgo = yearsAgo;
  return out.ok;
}

bool fetchNationalDays(std::vector<String> &out) {
  JsonDocument filter;
  filter["today"] = true;
  JsonDocument d;
  if (httpGetJson("https://raw.githubusercontent.com/jvivona/tidbyt-data/refs/heads/main/nationalday/nationalday.json",
                  d, &filter) != 200)
    return false;
  out.clear();
  for (JsonVariantConst v : d["today"].as<JsonArrayConst>()) {
    String s = v.as<String>();
    if (s.length()) out.push_back(s);
  }
  return !out.empty();
}

bool fetchHackerNews(std::vector<String> &titles, int n) {
  JsonDocument ids;
  if (httpGetJson("https://hacker-news.firebaseio.com/v0/topstories.json", ids) != 200) return false;
  std::vector<String> got;
  JsonDocument filter;
  filter["title"] = true;
  filter["score"] = true;
  for (int i = 0; i < n && i < (int)ids.size(); i++) {
    JsonDocument it;
    String url = "https://hacker-news.firebaseio.com/v0/item/" + String((long)(ids[i] | 0L)) + ".json";
    if (httpGetJson(url, it, &filter) != 200) continue;
    String t = it["title"] | "";
    if (t.length()) got.push_back(t);
  }
  if (got.empty()) return false;
  titles = got;
  return true;
}

bool parseSurf(const JsonDocument &d, SurfReport &out) {
  JsonObjectConst c = d["current"];
  if (c.isNull() || c["wave_height"].isNull()) return false;
  out.waveFt = (c["wave_height"] | 0.0f) * 3.28084f;
  out.periodS = c["wave_period"] | 0.0f;
  out.dirDeg = (int)(c["wave_direction"] | 0.0f);
  float sst = c["sea_surface_temperature"] | NAN;
  out.waterF = std::isnan(sst) ? -999 : (int)lroundf(sst * 9 / 5 + 32);
  out.maxFtToday = (d["daily"]["wave_height_max"][0] | 0.0f) * 3.28084f;
  out.ok = true;
  return true;
}

bool fetchSurf(float lat, float lon, SurfReport &out) {
  char url[300];
  snprintf(url, sizeof(url),
           "https://marine-api.open-meteo.com/v1/marine?latitude=%.4f&longitude=%.4f"
           "&current=wave_height,wave_period,wave_direction,sea_surface_temperature"
           "&daily=wave_height_max&forecast_days=1&timezone=auto",
           lat, lon);
  JsonDocument d;
  if (httpGetJson(url, d) != 200) return false;
  return parseSurf(d, out);
}

bool fetchIss(IssFix &out) {
  JsonDocument d;
  if (httpGetJson("https://api.wheretheiss.at/v1/satellites/25544", d) != 200) return false;
  if (d["latitude"].isNull()) return false;
  float lon = d["longitude"] | 0.0f;
  if (out.ok && out.at) {  // learn the drift so the dot keeps moving between fetches
    float mins = (millis() - out.at) / 60000.0f;
    float dl = lon - out.lon;
    if (dl < -180) dl += 360;
    if (dl > 180) dl -= 360;
    if (mins > 0.2f && fabsf(dl / mins) < 10) out.dLonPerMin = dl / mins;
  }
  out.lat = d["latitude"] | 0.0f;
  out.lon = lon;
  out.at = millis();
  out.ok = true;
  return true;
}

bool parseLastResult(const JsonDocument &d, const String &team, time_t now, GameResult &out) {
  bool found = false;
  for (JsonObjectConst e : d["events"].as<JsonArrayConst>()) {
    JsonObjectConst c = e["competitions"][0];
    if (!(c["status"]["type"]["completed"] | false)) continue;
    time_t when = parseIsoUtc(e["date"] | "");
    if (!when || when > now) continue;
    if (found && when <= out.when) continue;
    GameResult r;
    r.when = when;
    for (JsonObjectConst t : c["competitors"].as<JsonArrayConst>()) {
      String abbr = t["team"]["abbreviation"] | "";
      int score = t["score"]["value"] | (t["score"] | 0);
      if (abbr.equalsIgnoreCase(team)) {
        r.us = abbr;
        r.usScore = score;
        r.won = t["winner"] | false;
      } else {
        r.them = abbr;
        r.themScore = score;
      }
    }
    if (!r.us.length()) continue;
    r.ok = true;
    out = r;
    found = true;
  }
  return found;
}

bool fetchLastResult(const String &team, time_t now, GameResult &out) {
  JsonDocument filter;
  JsonObject ev = filter["events"][0].to<JsonObject>();
  ev["date"] = true;
  JsonObject c = ev["competitions"][0].to<JsonObject>();
  c["status"]["type"]["completed"] = true;
  JsonObject t = c["competitors"][0].to<JsonObject>();
  t["team"]["abbreviation"] = true;
  t["winner"] = true;
  t["score"] = true;
  String base = "https://site.api.espn.com/apis/site/v2/sports/baseball/mlb/teams/" + team + "/schedule";
  base.toLowerCase();
  GameResult best;
  bool any = false;
  struct tm lt;
  localtime_r(&now, &lt);
  // regular season, plus the postseason in October/November
  for (int pass = 0; pass < 2; pass++) {
    if (pass == 1 && !(lt.tm_mon == 9 || lt.tm_mon == 10)) break;
    String url = base + (pass ? "?seasontype=3" : "");
    JsonDocument d;
    if (httpGetJson(url, d, &filter) != 200) continue;
    GameResult r;
    if (parseLastResult(d, team, now, r) && (!any || r.when > best.when)) {
      best = r;
      any = true;
    }
  }
  if (any) out = best;
  return any;
}
