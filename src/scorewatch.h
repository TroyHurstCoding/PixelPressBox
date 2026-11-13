// scorewatch.h - notice scoring plays so we can flash + play a sound.
#pragma once
#include "model.h"
#include <map>

struct ScoreEvent {
  Game game;
  bool home;        // which side scored
  int delta;
  String label;     // "TOUCHDOWN", "FIELD GOAL", "RUN!" ...
  String sound;     // base sound name: "td", "fg", "safety", "xp", "run", "score"
};

class ScoreWatch {
 public:
  std::vector<ScoreEvent> update(const String &league, const std::vector<Game> &games);

 private:
  std::map<String, std::pair<int, int>> last;
};

bool shouldCelebrate(const ScoreEvent &e);
