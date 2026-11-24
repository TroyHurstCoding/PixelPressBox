#include "fantasy.h"
#include <cctype>
#include <cmath>
#include <cstring>
#include "net.h"

// ---------------------------------------------------------------- rules
// The owner's league (OctoBoxVR server/fantasy/scoring.json): half PPR, 5-point passing TDs, 0.04 per passing yard,
// -2 per interception and lost fumble, kickers 3 / 1 with -2 / -1 for misses, 2-point bonuses at 400 passing,
// 150 rushing and 150 receiving yards, and a points-allowed ladder for team defences.
void fantasyDefaultRules(std::vector<std::pair<String, float>> &rules, std::vector<std::pair<int, float>> &tiers) {
  rules = {{"passYds", 0.04f}, {"passTD", 5}, {"passInt", -2}, {"rushYds", 0.1f}, {"rushTD", 6},
           {"recYds", 0.1f},   {"recTD", 6},  {"rec", 0.5f},   {"fumLost", -2},  {"fgMade", 3},
           {"xpMade", 1},      {"fgMiss", -2}, {"xpMiss", -1}, {"kickRetTD", 6}, {"puntRetTD", 6},
           {"defTD", 6},       {"tackleSolo", 0}, {"tackleTotal", 0}, {"sack", 1}, {"intDef", 1},
           {"passDefended", 0}, {"fumRecovered", 1},
           {"dstSack", 1},     {"dstInt", 2}, {"dstFumRec", 2}, {"dstTD", 6}, {"dstSafety", 2}, {"dstRetTD", 0},
           {"pass400", 2},     {"rush150", 2}, {"rec150", 2}};
  tiers = {{0, 10}, {6, 7}, {13, 4}, {20, 1}, {27, 0}, {34, -1}, {999, -4}};
}

FantasyRules fantasyRules() {
  FantasyRules r;
  std::vector<std::pair<String, float>> def;
  fantasyDefaultRules(def, r.dstTiers);
  for (auto &kv : def) r.pts[kv.first] = kv.second;
  for (auto &kv : CFG.fptsRules) r.pts[kv.first] = kv.second;  // the phone page's values win
  if (!CFG.fptsDstTiers.empty()) r.dstTiers = CFG.fptsDstTiers;
  return r;
}

// ---------------------------------------------------------------- box score
static float num(JsonVariantConst v) {
  if (v.is<float>() || v.is<int>()) return v.as<float>();
  const char *s = v.as<const char *>();
  if (!s) return 0;
  String t = s;
  t.replace(",", "");
  return atof(t.c_str());
}
// "12/33" or "2-16" -> the two numbers
static void pairOf(JsonVariantConst v, float &a, float &b) {
  const char *s = v.as<const char *>();
  a = b = 0;
  if (!s) return;
  const char *sep = strpbrk(s, "/-");
  a = atof(s);
  if (sep) b = atof(sep + 1);
}

static void addCategory(const char *cat, JsonArrayConst keys, JsonArrayConst vals, StatMap &s) {
  auto idx = [&](const char *k) {
    int i = 0;
    for (JsonVariantConst x : keys) {
      if (strcmp(x | "", k) == 0) return i;
      i++;
    }
    return -1;
  };
  auto g = [&](const char *k) {
    int i = idx(k);
    return i < 0 || i >= (int)vals.size() ? 0.0f : num(vals[i]);
  };
  auto add = [&](const char *k, float v) { s[k] += v; };
  if (!strcmp(cat, "passing")) {
    add("passYds", g("passingYards"));
    add("passTD", g("passingTouchdowns"));
    add("passInt", g("interceptions"));
    float c, a;
    int i = idx("completions/passingAttempts");
    if (i >= 0) {
      pairOf(vals[i], c, a);
      add("cmp", c);
      add("att", a);
    }
  } else if (!strcmp(cat, "rushing")) {
    add("rushYds", g("rushingYards"));
    add("rushTD", g("rushingTouchdowns"));
    add("carries", g("rushingAttempts"));
  } else if (!strcmp(cat, "receiving")) {
    add("recYds", g("receivingYards"));
    add("recTD", g("receivingTouchdowns"));
    add("rec", g("receptions"));
    add("targets", g("receivingTargets"));
  } else if (!strcmp(cat, "fumbles")) {
    add("fumLost", g("fumblesLost"));
    add("fumRecovered", g("fumblesRecovered"));
  } else if (!strcmp(cat, "kicking")) {
    float m, a;
    int i = idx("fieldGoalsMade/fieldGoalAttempts");
    if (i >= 0) {
      pairOf(vals[i], m, a);
      add("fgMade", m);
      add("fgAtt", a);
    }
    i = idx("extraPointsMade/extraPointAttempts");
    if (i >= 0) {
      pairOf(vals[i], m, a);
      add("xpMade", m);
      add("xpAtt", a);
    }
  } else if (!strcmp(cat, "kickReturns")) {
    add("kickRetTD", g("kickReturnTouchdowns"));
  } else if (!strcmp(cat, "puntReturns")) {
    add("puntRetTD", g("puntReturnTouchdowns"));
  } else if (!strcmp(cat, "defensive")) {
    add("tackleTotal", g("totalTackles"));
    add("tackleSolo", g("soloTackles"));
    add("sack", g("sacks"));
    add("passDefended", g("passesDefended"));
    add("defTD", g("defensiveTouchdowns"));
  } else if (!strcmp(cat, "interceptions")) {
    add("intDef", g("interceptions"));
    add("defTD", g("interceptionTouchdowns"));
  }
}

void fantasyAddBox(const JsonDocument &d, std::map<String, StatMap> &byAthlete, std::map<String, StatMap> &byTeam) {
  for (JsonObjectConst tb : d["boxscore"]["players"].as<JsonArrayConst>()) {
    String team = tb["team"]["abbreviation"] | "";
    for (JsonObjectConst cat : tb["statistics"].as<JsonArrayConst>()) {
      const char *name = cat["name"] | "";
      JsonArrayConst keys = cat["keys"];
      for (JsonObjectConst a : cat["athletes"].as<JsonArrayConst>()) {
        String id = a["athlete"]["id"] | "";
        if (!id.length()) continue;
        StatMap one;
        addCategory(name, keys, a["stats"], one);
        for (auto &kv : one) {
          byAthlete[id][kv.first] += kv.second;
          if (team.length()) byTeam[team][kv.first] += kv.second;
        }
      }
    }
  }
  // safeties are not in the box score, only in the scoring plays
  for (JsonObjectConst sp : d["scoringPlays"].as<JsonArrayConst>()) {
    String t = sp["type"]["abbreviation"] | "";
    t.toUpperCase();
    String ab = sp["team"]["abbreviation"] | "";
    if (t == "SF" && ab.length()) byTeam[ab]["safeties"] += 1;
  }
}

static float stat(const StatMap &s, const char *k) {
  auto it = s.find(k);
  return it == s.end() ? 0 : it->second;
}

float fantasyScore(const StatMap &s, const FantasyRules &r) {
  float p = 0;
  for (const char *k : {"passYds", "passTD", "passInt", "rushYds", "rushTD", "recYds", "recTD", "rec", "fumLost",
                        "fgMade", "xpMade", "kickRetTD", "puntRetTD", "defTD", "tackleSolo", "tackleTotal", "sack",
                        "intDef", "passDefended", "fumRecovered"})
    p += stat(s, k) * r.get(k);
  p += (stat(s, "fgAtt") - stat(s, "fgMade")) * r.get("fgMiss");
  p += (stat(s, "xpAtt") - stat(s, "xpMade")) * r.get("xpMiss");
  // yardage bonuses: any rule named pass300, pass400, rush100, rec150 ... pays once at that many yards
  for (auto &kv : r.pts) {
    const char *k = kv.first.c_str();
    const char *yk = nullptr;
    int skip = 0;
    if (!strncmp(k, "pass", 4)) yk = "passYds", skip = 4;
    else if (!strncmp(k, "rush", 4)) yk = "rushYds", skip = 4;
    else if (!strncmp(k, "rec", 3)) yk = "recYds", skip = 3;
    if (!yk || !isdigit((unsigned char)k[skip])) continue;
    if (stat(s, yk) >= atoi(k + skip)) p += kv.second;
  }
  return roundf(p * 100) / 100;
}

float fantasyDstScore(const StatMap &a, int pa, const FantasyRules &r) {
  float p = stat(a, "sack") * r.get("dstSack") + stat(a, "intDef") * r.get("dstInt") +
            stat(a, "fumRecovered") * r.get("dstFumRec") + stat(a, "defTD") * r.get("dstTD") +
            stat(a, "safeties") * r.get("dstSafety") + (stat(a, "kickRetTD") + stat(a, "puntRetTD")) * r.get("dstRetTD");
  if (pa >= 0)
    for (auto &t : r.dstTiers)
      if (pa <= t.first) {
        p += t.second;
        break;
      }
  return roundf(p * 100) / 100;
}

static String n0(float v) { return String((int)lroundf(v)); }

String fantasyStatLine(const StatMap &s) {
  String b;
  auto part = [&](const String &x) {
    if (b.length()) b += "  ";
    b += x;
  };
  if (stat(s, "att") > 0) {
    String x = n0(stat(s, "cmp")) + "/" + n0(stat(s, "att")) + " " + n0(stat(s, "passYds")) + "yd";
    if (stat(s, "passTD")) x += " " + n0(stat(s, "passTD")) + "TD";
    if (stat(s, "passInt")) x += " " + n0(stat(s, "passInt")) + "INT";
    part(x);
  }
  if (stat(s, "carries") > 0) {
    String x = n0(stat(s, "carries")) + "car " + n0(stat(s, "rushYds")) + "yd";
    if (stat(s, "rushTD")) x += " " + n0(stat(s, "rushTD")) + "TD";
    part(x);
  }
  if (stat(s, "rec") > 0 || stat(s, "targets") > 0) {
    String x = n0(stat(s, "rec")) + "/" + n0(stat(s, "targets")) + " " + n0(stat(s, "recYds")) + "yd";
    if (stat(s, "recTD")) x += " " + n0(stat(s, "recTD")) + "TD";
    part(x);
  }
  if (stat(s, "fgAtt") > 0 || stat(s, "xpAtt") > 0)
    part("FG " + n0(stat(s, "fgMade")) + "/" + n0(stat(s, "fgAtt")) + " XP " + n0(stat(s, "xpMade")) + "/" +
         n0(stat(s, "xpAtt")));
  if (stat(s, "fumLost")) part(n0(stat(s, "fumLost")) + "FUM");
  if (!b.length() && stat(s, "tackleTotal")) part(n0(stat(s, "tackleTotal")) + "tkl");
  return b;
}

String fantasyDstLine(const StatMap &a, int pa) {
  String b;
  auto part = [&](const String &x) {
    if (b.length()) b += "  ";
    b += x;
  };
  if (pa >= 0) part("PA " + String(pa));
  if (stat(a, "sack")) part(n0(stat(a, "sack")) + "SK");
  if (stat(a, "intDef")) part(n0(stat(a, "intDef")) + "INT");
  if (stat(a, "fumRecovered")) part(n0(stat(a, "fumRecovered")) + "FR");
  if (stat(a, "defTD")) part(n0(stat(a, "defTD")) + "TD");
  if (stat(a, "safeties")) part(n0(stat(a, "safeties")) + "SF");
  return b;
}

// ---------------------------------------------------------------- the board
void fantasySummaryFilter(JsonDocument &filter) {
  JsonObject p = filter["boxscore"]["players"][0].to<JsonObject>();
  p["team"]["abbreviation"] = true;
  JsonObject st = p["statistics"][0].to<JsonObject>();
  st["name"] = true;
  st["keys"] = true;
  st["athletes"][0]["athlete"]["id"] = true;
  st["athletes"][0]["stats"] = true;
  JsonObject sp = filter["scoringPlays"][0].to<JsonObject>();
  sp["type"]["abbreviation"] = true;
  sp["team"]["abbreviation"] = true;
}

bool fantasyFetchSummary(const String &eventId, JsonDocument &doc) {
  JsonDocument filter;
  fantasySummaryFilter(filter);
  return httpGetJson("https://site.api.espn.com/apis/site/v2/sports/football/nfl/summary?event=" + eventId, doc,
                     &filter) == 200;
}

FptsBoard fantasyBuild(const std::vector<Game> &nfl, std::function<bool(const String &, JsonDocument &)> fetchSummary,
                       FantasyCache &cache) {
  FptsBoard b;
  b.meLabel = CFG.fptsMe;
  b.oppLabel = CFG.fptsOpp;
  b.week = CFG.fptsWeek;
  FantasyRules rules = fantasyRules();
  auto gameOf = [&](const String &team) -> const Game * {
    for (auto &g : nfl)
      if (g.away.abbr == team || g.home.abbr == team) return &g;
    return nullptr;
  };
  // download (or reuse) the box score of every started game that has one of our players
  std::map<String, bool> have;
  for (auto &p : CFG.fptsPlayers) {
    const Game *g = gameOf(p.team);
    if (!g || g->state == 'p' || have.count(g->id)) continue;
    if (g->state == 'f' && cache.final.count(g->id)) {
      have[g->id] = true;
      continue;
    }
    JsonDocument d;
    if (!fetchSummary(g->id, d)) continue;
    std::map<String, StatMap> ath, teams;
    fantasyAddBox(d, ath, teams);
    cache.athletes[g->id] = std::move(ath);  // live games are replaced next time; finals stay
    cache.teams[g->id] = std::move(teams);
    if (g->state == 'f') cache.final[g->id] = true;
    have[g->id] = true;
  }
  // forget games that are no longer on the scoreboard (last week)
  for (auto it = cache.athletes.begin(); it != cache.athletes.end();) {
    bool on = false;
    for (auto &g : nfl) on |= g.id == it->first;
    if (!on) {
      cache.teams.erase(it->first);
      cache.final.erase(it->first);
      it = cache.athletes.erase(it);
    } else {
      ++it;
    }
  }
  static const StatMap none;
  for (auto &p : CFG.fptsPlayers) {
    FptsLine l;
    l.name = p.name;
    l.pos = p.pos;
    l.team = p.team;
    l.side = p.side;
    const Game *g = gameOf(p.team);
    if (g) {
      l.state = g->state;
      b.anyLive |= g->state == 'i';
      b.anyStarted |= g->state != 'p';
      const Team &us = g->away.abbr == p.team ? g->away : g->home;
      const Team &them = g->away.abbr == p.team ? g->home : g->away;
      bool dst = p.pos == "DST";
      if (g->state == 'i' && g->poss.length()) l.hasBall = dst ? g->poss != us.id : g->poss == us.id;
      if (g->state != 'p' && have.count(g->id)) {
        if (dst) {
          auto &tm = cache.teams[g->id];
          auto it = tm.find(p.team);
          const StatMap &s = it == tm.end() ? none : it->second;
          l.pts = fantasyDstScore(s, them.score, rules);
          l.stats = fantasyDstLine(s, them.score);
        } else {
          auto &am = cache.athletes[g->id];
          auto it = am.find(p.id);
          const StatMap &s = it == am.end() ? none : it->second;
          l.pts = fantasyScore(s, rules);
          l.stats = fantasyStatLine(s);
        }
      }
    }
    (l.side ? b.opp : b.me) += l.pts;
    b.lines.push_back(l);
  }
  b.me = roundf(b.me * 100) / 100;
  b.opp = roundf(b.opp * 100) / 100;
  b.ok = !b.lines.empty();
  return b;
}
