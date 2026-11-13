#include "scorewatch.h"

static void labelFor(const String &lg, int d, String &label, String &sound) {
  if (lg == "nfl") {
    if (d >= 6) { label = "TOUCHDOWN"; sound = "td"; return; }
    if (d == 3) { label = "FIELD GOAL"; sound = "fg"; return; }
    if (d == 2) { label = "SAFETY/2PT"; sound = "safety"; return; }
    if (d == 1) { label = "EXTRA PT"; sound = "xp"; return; }
  }
  if (lg == "mlb") {
    label = d == 1 ? "RUN!" : String(d) + " RUNS!";
    sound = "run";
    return;
  }
  label = "+" + String(d);
  sound = "score";
}

std::vector<ScoreEvent> ScoreWatch::update(const String &league, const std::vector<Game> &games) {
  std::vector<ScoreEvent> evs;
  std::map<String, bool> seen;
  for (auto &g : games) {
    String k = g.key();
    seen[k] = true;
    std::pair<int, int> now{g.away.score, g.home.score};
    auto it = last.find(k);
    bool first = (it == last.end());
    std::pair<int, int> prev = first ? now : it->second;
    last[k] = now;
    if (first || g.state != 'i') continue;
    for (int side = 0; side < 2; side++) {
      int d = side ? now.second - prev.second : now.first - prev.first;
      if (d <= 0) continue;
      ScoreEvent e;
      e.game = g;
      e.home = side == 1;
      e.delta = d;
      labelFor(league, d, e.label, e.sound);
      evs.push_back(e);
    }
  }
  for (auto it = last.begin(); it != last.end();) {  // drop games no longer on this league's board
    if (it->first.startsWith(league + ":") && !seen.count(it->first)) it = last.erase(it);
    else ++it;
  }
  return evs;
}

bool shouldCelebrate(const ScoreEvent &e) {
  const String &lg = e.game.league;
  if (!Settings::has(CFG.celebrate, lg)) return false;
  const String &abbr = e.home ? e.game.home.abbr : e.game.away.abbr;
  if (CFG.isFavorite(lg, abbr)) return true;
  return Settings::has(CFG.celebrateAll, lg);
}
