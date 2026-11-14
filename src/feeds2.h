// feeds2.h - batch 3 data cards: trivia, facts & quotes, Billboard throwback, national day, Hacker News,
// surf (Open-Meteo marine), space station position, and the last result for "Fly the W".
#pragma once
#include <ArduinoJson.h>
#include "model.h"

bool fetchTrivia(std::vector<TriviaQ> &out);                 // 10 multiple-choice questions (OpenTDB)
bool fetchFact(FactQuote &out);                              // uselessfacts.jsph.pl
bool fetchQuote(FactQuote &out);                             // zenquotes.io (credited on the card)
bool fetchChart(time_t now, int yearsAgo, ChartWeek &out);   // mhollingshead/billboard-hot-100 on GitHub
bool fetchNationalDays(std::vector<String> &out);            // jvivona/tidbyt-data national day list
bool fetchHackerNews(std::vector<String> &titles, int n = 5);
bool fetchSurf(float lat, float lon, SurfReport &out);
bool fetchIss(IssFix &out);                                  // wheretheiss.at
bool fetchLastResult(const String &team, time_t now, GameResult &out);  // ESPN team schedule (MLB)

// ---- pure helpers (unit-tested)
String urlDecode(const String &s);                           // %20 -> ' ' (OpenTDB's url3986 encoding)
String chartDateFor(time_t now, int yearsAgo);               // the Saturday-dated chart for this week N years ago
bool parseSurf(const JsonDocument &d, SurfReport &out);
bool parseLastResult(const JsonDocument &d, const String &team, time_t now, GameResult &out);
