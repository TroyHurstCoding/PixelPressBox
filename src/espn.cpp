#include <algorithm>
#include "espn.h"
#include "net.h"

static const char *sportFor(const String &lg) {
  if (lg == "nfl") return "football";
  if (lg == "nba") return "basketball";
  return "baseball";
}

static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

time_t parseIsoUtc(const char *s) {
  // "2026-11-13T01:15Z" or with seconds
  if (!s) return 0;
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0;
  int n = sscanf(s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec);
  if (n < 5) return 0;
  return (time_t)(daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + sec);
}

uint32_t hexColor(const char *s, uint32_t fallback) {
  if (!s || !*s) return fallback;
  return (uint32_t)strtoul(s, nullptr, 16);
}

static int toInt(JsonVariantConst v) {
  if (v.is<int>()) return v.as<int>();
  const char *s = v.as<const char *>();
  return s ? atoi(s) : 0;
}

bool fetchScoreboard(const String &league, std::vector<Game> &out) {
  // Only keep the fields we use: a 250 KB response becomes ~10 KB in memory.
  JsonDocument filter;
  JsonObject ev = filter["events"][0].to<JsonObject>();
  ev["id"] = true;
  ev["date"] = true;
  JsonObject comp = ev["competitions"][0].to<JsonObject>();
  JsonObject c = comp["competitors"][0].to<JsonObject>();
  c["id"] = true;
  c["homeAway"] = true;
  c["score"] = true;
  c["team"]["abbreviation"] = true;
  c["team"]["color"] = true;
  c["team"]["alternateColor"] = true;
  c["records"][0]["summary"] = true;
  JsonObject sit = comp["situation"].to<JsonObject>();
  for (const char *k : {"possession", "shortDownDistanceText", "downDistanceText", "possessionText", "isRedZone",
                        "homeTimeouts", "awayTimeouts", "outs", "balls", "strikes", "onFirst", "onSecond", "onThird"})
    sit[k] = true;
  sit["lastPlay"]["text"] = true;
  ev["status"]["period"] = true;
  ev["status"]["displayClock"] = true;
  ev["status"]["type"]["state"] = true;
  ev["status"]["type"]["shortDetail"] = true;

  String url = String("https://site.api.espn.com/apis/site/v2/sports/") + sportFor(league) + "/" + league +
               "/scoreboard";
  JsonDocument doc;
  int code = httpGetJson(url, doc, &filter);
  if (code != 200) {
    Serial.printf("[espn] %s scoreboard failed: %d\n", league.c_str(), code);
    return false;
  }
  out.clear();
  for (JsonObjectConst e : doc["events"].as<JsonArrayConst>()) {
    Game g;
    g.league = league;
    g.id = e["id"].as<String>();
    g.start = parseIsoUtc(e["date"]);
    JsonObjectConst cp = e["competitions"][0];
    for (JsonObjectConst t : cp["competitors"].as<JsonArrayConst>()) {
      Team tm;
      tm.id = t["id"].as<String>();
      tm.abbr = t["team"]["abbreviation"] | "?";
      tm.color = hexColor(t["team"]["color"], 0xFFFFFF);
      tm.alt = hexColor(t["team"]["alternateColor"], 0x000000);
      tm.score = toInt(t["score"]);
      tm.record = t["records"][0]["summary"] | "";
      if (strcmp(t["homeAway"] | "home", "away") == 0) g.away = tm; else g.home = tm;
    }
    const char *st = e["status"]["type"]["state"] | "pre";
    g.state = (strcmp(st, "in") == 0) ? 'i' : (strcmp(st, "post") == 0 ? 'f' : 'p');
    g.detail = e["status"]["type"]["shortDetail"] | "";
    g.clock = e["status"]["displayClock"] | "";
    g.period = e["status"]["period"] | 0;
    JsonObjectConst s = cp["situation"];
    if (!s.isNull()) {
      g.hasSit = true;
      g.poss = s["possession"].as<String>();
      g.down = s["shortDownDistanceText"] | (s["downDistanceText"] | "");
      g.redzone = s["isRedZone"] | false;
      g.spot = s["possessionText"] | "";
      g.lastPlay = cleanPlay(s["lastPlay"]["text"] | "");
      g.awayTO = s["awayTimeouts"] | -1;
      g.homeTO = s["homeTimeouts"] | -1;
      g.outs = s["outs"] | 0;
      g.balls = s["balls"] | 0;
      g.strikes = s["strikes"] | 0;
      g.on1 = s["onFirst"] | false;
      g.on2 = s["onSecond"] | false;
      g.on3 = s["onThird"] | false;
    }
    out.push_back(g);
  }
  return true;
}

// ------------------------------------------------------------------ box scores / fantasy
struct Col { const char *label; const char *fmt; };
static bool skipZero(const char *lab) {
  for (const char *z : {"HR", "RBI", "R", "BB", "SB", "INT", "TD", "3PT"})
    if (strcmp(lab, z) == 0) return true;
  return false;
}

static String formatLine(const String &league, const String &category, JsonArrayConst labels,
                         JsonArrayConst stats) {
  static const Col NBA[] = {{"PTS", "%sp"}, {"REB", "%sr"}, {"AST", "%sa"}, {"3PT", "%s 3PT"}, {nullptr, nullptr}};
  static const Col PASS[] = {{"C/ATT", "%s"}, {"YDS", "%sy"}, {"TD", "%std"}, {"INT", "%sint"}, {nullptr, nullptr}};
  static const Col RUSH[] = {{"CAR", "%scar"}, {"YDS", "%sy"}, {"TD", "%std"}, {nullptr, nullptr}};
  static const Col REC[] = {{"REC", "%srec"}, {"YDS", "%sy"}, {"TD", "%std"}, {nullptr, nullptr}};
  static const Col KICK[] = {{"FG", "FG %s"}, {"XP", "XP %s"}, {nullptr, nullptr}};
  static const Col BAT[] = {{"H-AB", "%s"}, {"HR", "%sHR"}, {"RBI", "%sRBI"}, {"R", "%sR"}, {"BB", "%sBB"},
                            {"SB", "%sSB"}, {nullptr, nullptr}};
  static const Col PITCH[] = {{"IP", "%sIP"}, {"ER", "%sER"}, {"K", "%sK"}, {"H", "%sH"}, {nullptr, nullptr}};

  String cat = category;
  cat.toLowerCase();
  const Col *fmt = nullptr;
  if (league == "nba") fmt = NBA;
  else if (league == "nfl") {
    if (cat == "passing") fmt = PASS;
    else if (cat == "rushing") fmt = RUSH;
    else if (cat == "receiving") fmt = REC;
    else if (cat == "kicking") fmt = KICK;
  } else if (league == "mlb") {
    if (cat == "batting") fmt = BAT;
    else if (cat == "pitching") fmt = PITCH;
  }
  if (!fmt) return "";
  String outStr;
  for (const Col *c = fmt; c->label; c++) {
    int idx = -1, i = 0;
    for (JsonVariantConst l : labels) {
      if (strcmp(l | "", c->label) == 0) { idx = i; break; }
      i++;
    }
    if (idx < 0 || idx >= (int)stats.size()) continue;
    String v = stats[idx] | "";
    if (skipZero(c->label) && (v == "0" || v == "0-0" || v == "--")) continue;
    if (strcmp(c->label, "3PT") == 0) {
      int dash = v.indexOf('-');
      v = dash > 0 ? v.substring(0, dash) : v;
      if (v == "0" || v.length() == 0) continue;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), c->fmt, v.c_str());
    if (outStr.length()) outStr += ' ';
    outStr += buf;
  }
  return outStr;
}

static String slashLine(JsonArrayConst labels, JsonArrayConst stats) {
  String parts[3];
  const char *want[3] = {"AVG", "OBP", "SLG"};
  for (int k = 0; k < 3; k++) {
    int i = 0, idx = -1;
    for (JsonVariantConst l : labels) {
      if (strcmp(l | "", want[k]) == 0) { idx = i; break; }
      i++;
    }
    if (idx < 0 || idx >= (int)stats.size()) return "";
    parts[k] = stats[idx] | "";
  }
  return parts[0] + "/" + parts[1] + "/" + parts[2];
}

static bool nameMatch(const String &want, const String &got) {
  String w = want, g = got;
  w.toLowerCase();
  g.toLowerCase();
  if (w == g) return true;
  int sp = w.lastIndexOf(' ');
  String last = sp >= 0 ? w.substring(sp + 1) : w;
  return last.length() && (g.endsWith(" " + last) || g.indexOf(" " + last + " ") >= 0) && w[0] == g[0];
}

bool fetchBoxLines(const Game &g, const std::vector<FantasyPlayer> &want, std::map<String, StatLine> &out) {
  JsonDocument filter;
  JsonObject p = filter["boxscore"]["players"][0].to<JsonObject>();
  p["team"]["abbreviation"] = true;
  JsonObject st = p["statistics"][0].to<JsonObject>();
  st["name"] = true;
  st["type"] = true;
  st["labels"] = true;
  st["athletes"][0]["athlete"]["displayName"] = true;
  st["athletes"][0]["stats"] = true;
  st["athletes"][0]["didNotPlay"] = true;

  String url = String("https://site.api.espn.com/apis/site/v2/sports/") + sportFor(g.league) + "/" + g.league +
               "/summary?event=" + g.id;
  JsonDocument doc;
  if (httpGetJson(url, doc, &filter) != 200) return false;

  for (JsonObjectConst tb : doc["boxscore"]["players"].as<JsonArrayConst>()) {
    String abbr = tb["team"]["abbreviation"] | "";
    for (JsonObjectConst stat : tb["statistics"].as<JsonArrayConst>()) {
      String cat = stat["name"] | (stat["type"] | "");
      JsonArrayConst labels = stat["labels"];
      for (JsonObjectConst a : stat["athletes"].as<JsonArrayConst>()) {
        String nm = a["athlete"]["displayName"] | "";
        for (auto &fp : want) {
          if (fp.team != abbr || !nameMatch(fp.name, nm)) continue;
          StatLine &e = out[fp.name];
          if (a["didNotPlay"] | false) {
            if (!e.line.length()) e.line = "DNP";
            continue;
          }
          String line = formatLine(g.league, cat, labels, a["stats"]);
          if (line.length()) e.line = e.line.length() ? e.line + " | " + line : line;
          String lc = cat;
          lc.toLowerCase();
          if (g.league == "mlb" && lc == "batting") e.slash = slashLine(labels, a["stats"]);
        }
      }
    }
  }
  return true;
}

// ------------------------------------------------------------------ UFC schedule
bool fetchUfc(std::vector<FightEvent> &out) {
  JsonDocument filter;
  JsonObject cal = filter["leagues"][0]["calendar"][0].to<JsonObject>();
  cal["label"] = true;
  cal["startDate"] = true;
  cal["endDate"] = true;
  JsonDocument doc;
  int code = httpGetJson("https://site.api.espn.com/apis/site/v2/sports/mma/ufc/scoreboard", doc, &filter);
  if (code != 200) {
    Serial.printf("[espn] ufc schedule failed: %d\n", code);
    return false;
  }
  out.clear();
  for (JsonObjectConst c : doc["leagues"][0]["calendar"].as<JsonArrayConst>()) {
    FightEvent e;
    e.label = c["label"] | "";
    e.start = parseIsoUtc(c["startDate"] | "");
    e.end = parseIsoUtc(c["endDate"] | "");
    if (e.label.length() && e.start) out.push_back(e);
  }
  return true;
}

// ------------------------------------------------------------------ play text
String cleanPlay(const String &in) {
  String s = in;
  int cut = s.indexOf(" **");  // "** Injury Update ..." and similar notes
  if (cut > 0) s = s.substring(0, cut);
  s.trim();
  while (s.startsWith("(")) {  // "(Shotgun) ", "(No Huddle, Shotgun) ", "(14:32) "
    int close = s.indexOf(')');
    if (close < 0) break;
    s = s.substring(close + 1);
    s.trim();
  }
  if (s.endsWith(".")) s = s.substring(0, s.length() - 1);
  return s;
}

// ------------------------------------------------------------------ standings
static String statStr(JsonArrayConst stats, const char *name) {
  for (JsonObjectConst st : stats)
    if (strcmp(st["name"] | "", name) == 0) return st["displayValue"] | "";
  return "";
}

static void readGroup(const String &league, JsonObjectConst g, std::vector<StandGroup> &out) {
  StandGroup grp;
  grp.league = league;
  grp.name = g["name"] | "";
  for (JsonObjectConst e : g["standings"]["entries"].as<JsonArrayConst>()) {
    JsonArrayConst st = e["stats"];
    StandRow r;
    r.abbr = e["team"]["abbreviation"] | "?";
    r.wins = statStr(st, "wins").toInt();
    r.losses = statStr(st, "losses").toInt();
    r.ties = statStr(st, "ties").toInt();
    r.seed = statStr(st, "playoffSeed").toInt();
    r.pct = statStr(st, "winPercent").toFloat();
    r.gb = statStr(st, "gamesBehind");
    grp.rows.push_back(r);
  }
  // ESPN doesn't sort entries: best record first (NBA: by playoff seed when known)
  std::sort(grp.rows.begin(), grp.rows.end(), [&](const StandRow &a, const StandRow &b) {
    if (league == "nba" && a.seed && b.seed) return a.seed < b.seed;
    if (a.pct != b.pct) return a.pct > b.pct;
    return a.wins > b.wins;
  });
  if (!grp.rows.empty()) out.push_back(grp);
}

bool fetchStandings(const String &league, std::vector<StandGroup> &out) {
  JsonDocument filter;
  JsonObject top = filter["children"][0].to<JsonObject>();
  top["name"] = true;
  JsonObject ent = top["standings"]["entries"][0].to<JsonObject>();
  ent["team"]["abbreviation"] = true;
  ent["stats"][0]["name"] = true;
  ent["stats"][0]["displayValue"] = true;
  JsonObject kid = top["children"][0].to<JsonObject>();
  kid["name"] = true;
  JsonObject ent2 = kid["standings"]["entries"][0].to<JsonObject>();
  ent2["team"]["abbreviation"] = true;
  ent2["stats"][0]["name"] = true;
  ent2["stats"][0]["displayValue"] = true;
  String url = String("https://site.api.espn.com/apis/v2/sports/") + sportFor(league) + "/" + league + "/standings";
  if (league != "nba") url += "?level=3";  // divisions for NFL and MLB
  JsonDocument doc;
  int code = httpGetJson(url, doc, &filter);
  if (code != 200) {
    Serial.printf("[espn] %s standings failed: %d\n", league.c_str(), code);
    return false;
  }
  out.clear();
  for (JsonObjectConst c : doc["children"].as<JsonArrayConst>()) {
    if (c["children"].is<JsonArrayConst>())
      for (JsonObjectConst k : c["children"].as<JsonArrayConst>()) readGroup(league, k, out);
    else
      readGroup(league, c, out);
  }
  return !out.empty();
}
