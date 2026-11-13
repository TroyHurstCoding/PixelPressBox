#include "playlist.h"
#include <algorithm>

bool isFavoriteGame(const Game &g) {
  return CFG.isFavorite(g.league, g.away.abbr) || CFG.isFavorite(g.league, g.home.abbr);
}

int priorityOf(const Game &g, time_t now) {
  const bool fav = isFavoriteGame(g);
  const bool all = Settings::has(CFG.showAll, g.league);
  const long finalSecs = CFG.finalsHours * 3600L;
  const long preSecs = CFG.upcomingHours * 3600L;
  const long endedAgo = (long)(now - (g.start + 4 * 3600));  // finals have no end time; ~4h after start
  if (fav) {
    if (g.state == 'i') return PRI_FAV_LIVE;
    if (g.state == 'p' && g.start - now <= preSecs) return PRI_FAV_PRE;
    if (g.state == 'f' && endedAgo <= finalSecs) return PRI_FAV_FINAL;
    return 0;
  }
  if (all) {
    if (g.state == 'i') return PRI_ALL_LIVE;
    if (g.state == 'f' && endedAgo <= finalSecs) return PRI_ALL_FINAL;
    if (g.state == 'p' && g.start - now <= preSecs) return PRI_ALL_PRE;
  }
  return 0;
}

std::vector<Card> buildCards(const std::vector<Game> &games, time_t now, const std::vector<Card> &extra) {
  std::vector<Card> cards;
  for (auto &g : games) {
    int p = priorityOf(g, now);
    if (p <= 0) continue;
    Card c;
    c.kind = CARD_GAME;
    c.key = g.key();
    c.pri = p;
    c.dwell = (p == PRI_FAV_LIVE) ? CFG.dwellFavLive : CFG.dwellDefault;
    c.game = g;
    cards.push_back(c);
  }
  for (auto &e : extra) cards.push_back(e);
  std::stable_sort(cards.begin(), cards.end(), [](const Card &a, const Card &b) {
    if (a.pri != b.pri) return a.pri > b.pri;
    time_t sa = a.kind == CARD_GAME ? a.game.start : 0, sb = b.kind == CARD_GAME ? b.game.start : 0;
    return sa < sb;
  });
  return cards;
}

std::vector<Card> makePlaylist(const std::vector<Card> &cards) {
  std::vector<Card> take, rest;
  for (auto &c : cards) (c.pri >= PRI_FAV_LIVE ? take : rest).push_back(c);
  if (take.empty()) return rest;
  int every = std::max(1, CFG.takeoverMixEvery);
  if (every == 1 || rest.empty()) return take;
  std::vector<Card> out;
  size_t ti = 0;
  for (auto &o : rest) {  // (every-1) favorite cards, then 1 other card
    for (int k = 0; k < every - 1; k++) out.push_back(take[ti++ % take.size()]);
    out.push_back(o);
  }
  return out;
}

void Rotator::update(const std::vector<Card> &pl) {
  String cur = (!items.empty() && i < items.size()) ? items[i].key : String();
  items = pl;
  i = 0;
  if (cur.length())
    for (size_t k = 0; k < items.size(); k++)
      if (items[k].key == cur) { i = k; break; }
}

const Card *Rotator::current() {
  if (items.empty()) return nullptr;
  i %= items.size();
  return &items[i];
}

void Rotator::advance() {
  if (!items.empty()) i = (i + 1) % items.size();
}

bool Rotator::select(const String &key) {
  for (size_t k = 0; k < items.size(); k++)
    if (pinKeyOf(items[k]) == key) {
      i = k;
      return true;
    }
  return false;
}

String pinKeyOf(const Card &c) { return c.kind == CARD_CLOCK ? String("clock") : c.key; }

String cardLabel(const Card &c) {
  switch (c.kind) {
    case CARD_CLOCK: return "Clock";
    case CARD_GAME: {
      String l = c.game.away.abbr + " @ " + c.game.home.abbr;
      l.toUpperCase();
      return l + " (" + c.game.league + ")";
    }
    case CARD_FANTASY: return "Fantasy: " + c.title;
    case CARD_YAHOO: return "Yahoo fantasy matchup";
    case CARD_MEETING: return c.key == "mtg:now" ? "Current meeting" : "Next meeting";
    case CARD_TODO: return "To-do list";
    case CARD_COUNTDOWN: return "Countdown: " + c.title;
    case CARD_MUSIC: return "Now playing";
    case CARD_HOLIDAY: return "Holiday: " + c.text;
    case CARD_SUNMOON: return "Sun & moon";
    case CARD_PROGRESS: return "Date progress";
    case CARD_WEATHER: return "Weather details";
    case CARD_UFC: return "Next UFC event";
    case CARD_STANDINGS: return c.title + " standings";
    case CARD_HISTORY: return "Today in history";
    case CARD_WORD: return "Word of the day";
    case CARD_JOKE: return "Dad joke";
    case CARD_TETRIS: return "Tetris clock";
    case CARD_RAIN: return "Digital rain";
  }
  return c.key;
}
