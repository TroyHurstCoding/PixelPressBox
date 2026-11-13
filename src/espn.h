// espn.h - ESPN public scoreboard + box scores.
#pragma once
#include "model.h"
#include <map>

// Download today's scoreboard for a league. Returns false on network/parse failure.
bool fetchScoreboard(const String &league, std::vector<Game> &out);

struct StatLine {
  String line;   // "2-4 1HR 3RBI"
  String slash;  // ".302/.390/.610" (MLB only)
};

// Box-score stat lines for the given players in one game. name -> line
bool fetchBoxLines(const Game &g, const std::vector<FantasyPlayer> &want, std::map<String, StatLine> &out);

time_t parseIsoUtc(const char *s);
uint32_t hexColor(const char *s, uint32_t fallback);

// division/conference standings (ESPN standings API, sorted best first). NFL/MLB: divisions, NBA: conferences
bool fetchStandings(const String &league, std::vector<StandGroup> &out);
// "(Shotgun) J.Daniels pass ... ** Injury Update ..." -> "J.Daniels pass ..."
String cleanPlay(const String &s);

// upcoming UFC cards from ESPN's MMA calendar (about 37 KB, filtered down to labels and times)
bool fetchUfc(std::vector<FightEvent> &out);
