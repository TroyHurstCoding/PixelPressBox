// playlist.h - what to show, in what order, for how long (same rules as the CircuitPython version).
#pragma once
#include "model.h"

enum CardKind { CARD_GAME, CARD_FANTASY, CARD_YAHOO, CARD_CLOCK, CARD_MEETING, CARD_TODO, CARD_COUNTDOWN, CARD_MUSIC,
                CARD_HOLIDAY, CARD_SUNMOON, CARD_PROGRESS, CARD_WEATHER, CARD_UFC,
                CARD_STANDINGS, CARD_HISTORY, CARD_WORD, CARD_JOKE, CARD_TETRIS, CARD_RAIN,
                CARD_FIRE, CARD_BOUNCE, CARD_SNAKE, CARD_BREAKOUT, CARD_IMAGES, CARD_NIXIE, CARD_BIGCLOCK,
                CARD_WORDCLOCK };

constexpr int PRI_FAV_LIVE = 100, PRI_MEETING_SOON = 90, PRI_FAV_PRE = 60, PRI_FAV_FINAL = 55,
              PRI_MUSIC = 45, PRI_ALL_LIVE = 40, PRI_MEETING_LATER = 35, PRI_FANTASY = 30,
              PRI_TODO = 25, PRI_COUNTDOWN = 22, PRI_ALL_FINAL = 20, PRI_ALL_PRE = 15,
              PRI_HOLIDAY = 95, PRI_AMBIENT = 10;

struct Card {
  CardKind kind = CARD_GAME;
  String key;
  int pri = 0;
  int dwell = 5;
  Game game;          // CARD_GAME
  String title, text; // CARD_FANTASY
  bool live = false;
  // CARD_YAHOO
  String meName, oppName;
  float mePts = 0, oppPts = 0, meProj = 0, oppProj = 0;
  // desk cards
  std::vector<String> items;  // CARD_TODO
  int num = 0;                // CARD_COUNTDOWN days; CARD_HOLIDAY holiday index
  uint32_t color = 0xFFFFFF;
  time_t when = 0, until = 0; // CARD_MEETING start / end
};

bool isFavoriteGame(const Game &g);
int priorityOf(const Game &g, time_t now);
std::vector<Card> buildCards(const std::vector<Game> &games, time_t now, const std::vector<Card> &extra);
std::vector<Card> makePlaylist(const std::vector<Card> &cards);

// "clock:3" -> "clock" so a pinned clock survives playlist rebuilds
String pinKeyOf(const Card &c);
// human label for the phone page: "Clock", "LV @ KC", "AFC WEST standings"...
String cardLabel(const Card &c);

class Rotator {
 public:
  void update(const std::vector<Card> &pl);
  const Card *current();
  void advance();
  bool empty() const { return items.empty(); }
  // jump to the card with this key ("clock" matches any clock slot); false if it isn't in the playlist
  bool select(const String &key);
  const std::vector<Card> &all() const { return items; }

 private:
  std::vector<Card> items;
  size_t i = 0;
};
