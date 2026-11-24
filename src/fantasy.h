// fantasy.h - fantasy points for NFL players picked on the phone page, scored with the league's own rules.
// Same method as OctoBoxVR's fantasy-poll.mjs: ESPN's public scoreboard + game summaries (no account, no key).
// A team defence (D/ST) is built from that team's defensive lines, safeties and the opponent's score.
#pragma once
#include <ArduinoJson.h>
#include <functional>
#include <map>
#include "model.h"

typedef std::map<String, float> StatMap;  // "passYds" -> 254

// the league's scoring, from CFG.fptsRules / CFG.fptsDstTiers, with the defaults filled in
struct FantasyRules {
  std::map<String, float> pts;                       // points per unit, e.g. passTD 5
  std::vector<std::pair<int, float>> dstTiers;       // (max points allowed, points)
  float get(const char *k) const {
    auto it = pts.find(k);
    return it == pts.end() ? 0 : it->second;
  }
};
FantasyRules fantasyRules();                         // from CFG
void fantasyDefaultRules(std::vector<std::pair<String, float>> &rules, std::vector<std::pair<int, float>> &tiers);

// ---- pure scoring (unit-tested against a real summary)
// add one game summary's box score: per-athlete stats (by ESPN id) and per-team totals (by abbreviation)
void fantasyAddBox(const JsonDocument &summary, std::map<String, StatMap> &byAthlete, std::map<String, StatMap> &byTeam);
float fantasyScore(const StatMap &s, const FantasyRules &r);
float fantasyDstScore(const StatMap &team, int pointsAllowed, const FantasyRules &r);  // pointsAllowed < 0 = unknown
String fantasyStatLine(const StatMap &s);            // "21/30 254yd 2TD  4car 12yd"
String fantasyDstLine(const StatMap &team, int pointsAllowed);

// cache of finished games' box scores, so a final is downloaded once
struct FantasyCache {
  std::map<String, std::map<String, StatMap>> athletes, teams;  // game id -> ...
  std::map<String, bool> final;                                 // game id -> downloaded after the final whistle
};
// the whole board: fetchSummary(eventId, doc) downloads one game summary (filtered by fantasySummaryFilter)
FptsBoard fantasyBuild(const std::vector<Game> &nfl, std::function<bool(const String &, JsonDocument &)> fetchSummary,
                       FantasyCache &cache);
void fantasySummaryFilter(JsonDocument &filter);
// device side: download a summary (uses net.h)
bool fantasyFetchSummary(const String &eventId, JsonDocument &doc);
