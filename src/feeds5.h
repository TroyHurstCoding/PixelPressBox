// feeds5.h - batch 5 data: rocket launches (Launch Library 2), theme-park waits (queue-times.com), stock-token
// prices (Robinhood), ESPN news alerts, drive time (TomTom), Powerball (NY Open Data), pick'em scoring and the
// playoff race (from the standings the firmware already downloads).
// fetch* functions do the HTTPS download (via net.h httpGetJson) and call the parse* function; parse* functions
// are pure and unit-tested against saved responses in test/native/fixtures/.
#pragma once
#include <ArduinoJson.h>
#include <map>
#include "model.h"

// ---- launches: https://ll.thespacedevs.com/2.3.0/launches/upcoming/?limit=8  (normal mode: list mode has no
//      rocket/pad; 15 calls/hour free). Launches already flown (Success/Failure) are skipped.
bool fetchLaunches(std::vector<Launch> &out);
void parseLaunches(const JsonDocument &d, std::vector<Launch> &out);   // soonest first, skips ones already past

// ---- park waits: https://queue-times.com/parks/{id}/queue_times.json  ("Powered by Queue-Times.com")
bool fetchParkWaits(const String &parkId, const String &parkName, ParkWaits &out);
void parseParkWaits(const JsonDocument &d, ParkWaits &out);            // open rides only, longest wait first

// ---- stocks: https://api.robinhood.com/rhj/prices/{SYMBOL}  (no key; 15 s cache)
bool fetchStock(const String &symbol, StockQuote &q, time_t now);      // updates q in place (keeps spark/open/prevClose)
// applies one price response to q: price = (bid+ask)/2, day high/low, halted, at; appends to spark (max 64)
bool parseStock(const JsonDocument &d, StockQuote &q, time_t now);
// trading-day rollover (US Eastern date of `now` differs from q.at's): prevClose = last price, open = 0, spark cleared
void stockRollover(StockQuote &q, time_t now);
// list for the phone page: https://api.robinhood.com/rhj/assets -> [["AAPL","Apple Inc."], ...] sorted by symbol
bool fetchStockList(std::vector<std::pair<String, String>> &out);
void parseStockList(const JsonDocument &d, std::vector<std::pair<String, String>> &out);

// ---- news: https://site.api.espn.com/apis/site/v2/sports/{sport}/{league}/news?limit=40
// teams: (league, abbr) to watch; players: fantasy roster names; keywords: lower-case words/phrases.
// An article matches if it's tagged with a watched team or athlete id, names a watched player, or (for a watched
// team's league) the headline contains a keyword AND a watched team/player. `why` says what matched.
struct NewsWatch {
  std::vector<std::pair<String, String>> teams;   // ("nfl","BUF")
  std::vector<String> athleteIds, playerNames;    // ESPN ids / "Josh Allen"
  std::vector<String> keywords;
};
bool fetchNews(const String &league, const NewsWatch &w, std::vector<NewsItem> &out);
void parseNews(const JsonDocument &d, const String &league, const NewsWatch &w, std::vector<NewsItem> &out);

// ---- drive time (TomTom): https://api.tomtom.com/routing/1/calculateRoute/{lat},{lon}:{lat},{lon}/json?traffic=true&key=KEY
bool fetchDriveTime(const String &key, float fromLat, float fromLon, float toLat, float toLon, DriveTime &out);
void parseDriveTime(const JsonDocument &d, DriveTime &out);
// address -> lat/lon: https://api.tomtom.com/search/2/geocode/{query}.json?limit=1&key=KEY
bool geocodeTomTom(const String &key, const String &address, float &lat, float &lon);
bool parseGeocode(const JsonDocument &d, float &lat, float &lon);

// ---- Powerball: https://data.ny.gov/resource/d6yy-54nr.json?$order=draw_date%20DESC&$limit=1
bool fetchLotto(LottoDraw &out, time_t now);
void parseLotto(const JsonDocument &d, LottoDraw &out);
time_t nextPowerballDraw(time_t now);   // Mon/Wed/Sat 10:59 PM US Eastern, as UTC

// ---- pick'em: fills each pick's result from the scoreboard (W/L/T once the game is final)
// returns true if any result changed. Picks for games not in `games` are left as they are.
bool pickemScore(std::vector<PickemPick> &picks, const std::map<String, std::vector<Game>> &games);

// ---- playoff race: a one-line summary for a team in its standings table:
// "1ST SEED", "2.5 GB", "MAGIC 4"... Magic number for a division leader = games + 1 - (our wins) - (2nd's losses)
// using season length per league (nfl 17, nba 82, mlb 162, nhl 82; -1 when unknown)
struct PlayoffLine {
  bool ok = false;
  int rank = 0;            // place in the group (1 = leader)
  int seed = 0;            // playoff seed when ESPN gives one
  String gb;               // games back ("-" leader)
  int magic = -1;          // leader only; -1 unknown, 0 clinched
  String record;           // "10-3"
};
PlayoffLine playoffLine(const StandGroup &g, const String &team);
