#include "feeds.h"
#include "net.h"

bool grimHistory(const String &t) {
  String s = t;
  s.toLowerCase();
  static const char *W[] = {"kill", "death", "dead", "died", "murder", "massacre", "bomb", "crash", "attack",
                            "shooting", "genocide", "execut", "terror", "assassin", "earthquake", "explosion"};
  for (const char *w : W)
    if (s.indexOf(w) >= 0) return true;
  return false;
}

bool fetchHistory(int month, int day, bool gentle, std::vector<HistoryEvent> &out) {
  char url[96];
  snprintf(url, sizeof(url), "https://en.wikipedia.org/api/rest_v1/feed/onthisday/selected/%02d/%02d", month, day);
  JsonDocument filter;
  filter["selected"][0]["year"] = true;
  filter["selected"][0]["text"] = true;
  JsonDocument d;
  if (httpGetJson(url, d, &filter) != 200) return false;
  out.clear();
  for (JsonObjectConst e : d["selected"].as<JsonArrayConst>()) {
    HistoryEvent h;
    h.year = e["year"] | 0;
    h.text = e["text"] | "";
    h.text.replace("  ", " ");
    if (!h.text.length() || (gentle && grimHistory(h.text))) continue;
    out.push_back(h);
  }
  return !out.empty();
}

bool fetchWord(WordOfDay &w) {
  JsonDocument d;
  if (httpGetJson("https://wordoftheday.freeapi.me/", d) != 200) return false;
  w.word = d["word"] | "";
  w.pos = d["partOfSpeech"] | "";
  w.def = d["definition"] | "";
  w.ok = w.word.length() > 0;
  return w.ok;
}

bool fetchJoke(String &joke) {
  JsonDocument d;
  if (httpGetJson("https://icanhazdadjoke.com/", d) != 200) return false;  // Accept: application/json
  joke = d["joke"] | "";
  return joke.length() > 0;
}
