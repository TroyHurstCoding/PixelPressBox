// PixelPressbox - desk clock + live sports for the Seengreat RGB Matrix HUB75 S3 + 64x32 HUB75 panel.
//
// Cards: clock (time, date, weather, busy light, next-meeting countdown), Outlook meetings, to-dos
//        (phone page + flagged email), countdowns, Spotify now playing, sports scores, fantasy.
// Controls: thumbwheel up/down = next card, thumbwheel press = start/stop the focus timer
//           (BOOT tap = next, BOOT hold = focus timer). Phone page: next, favorites, busy, focus, to-dos.
// Settings:    config.json on the microSD card, or http://pixelpressbox.local from your phone
// WiFi:        first boot opens a "PixelPressbox-Setup" WiFi network; join it from your phone

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <map>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "board.h"
#include <map>
#include "desk.h"
#include "feeds.h"
#include "images.h"
#include "autoupdate.h"
#include "holidays.h"
#include "msgraph.h"
#include "net.h"
#include "spotify.h"
#include "todo.h"
#include "weather.h"
#include "espn.h"
#include "playlist.h"
#include <functional>
#include "render2.h"
#include "feeds2.h"
#include "feeds3.h"
#include "fantasy.h"
#include "render3.h"
#include "render5.h"
#include "feeds5.h"
#include "pet.h"
#include "motion.h"
#include "games.h"
#include "render.h"
#include "scorewatch.h"
#include "settings.h"
#include "sound.h"
#include "web.h"
#include "wifinet.h"
#include "yahoo.h"


static std::map<String, std::vector<Game>> games;
static std::map<String, uint32_t> nextFetch;  // millis()
static ScoreWatch watch;
static Rotator rot;
static std::vector<Card> fantasyCards;
static Card yahooCard;
static bool haveYahoo = false;
static uint32_t nextFantasy = 0, nextYahoo = 0;
static bool favOnly = false;
static bool soundReady = false;
static std::vector<ScoreEvent> pendingCelebrations;

// ---- threading: downloads run in netTask (core 0); the display/controls loop runs here (core 1).
// The main loop holds dataMtx all the time and lets go for a moment on every pump() tick; netTask downloads
// WITHOUT the lock and only takes it to hand over results, so the screen never waits on the network.
static SemaphoreHandle_t dataMtx = nullptr;
struct Lock {
  Lock() { xSemaphoreTakeRecursive(dataMtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(dataMtx); }
};
static volatile bool msLoginRequested = false, msLoginOk = false;
// desk clock state
static const uint32_t YELLOW_C = 0xFFC000;
static Weather wx;
static std::vector<Meeting> meetings;
static std::vector<String> flagged;
static NowPlaying np;
static std::vector<FightEvent> ufcEvents;
static std::map<String, std::vector<StandGroup>> standings;
static std::map<String, uint32_t> nextStandings;
static String presAvail, presActivity;
static uint32_t nextPresence = 0, nextJoke = 0;
static std::vector<HistoryEvent> history;
static size_t historyIdx = 0;
static WordOfDay wotd;
static String joke;
static int feedsDay = -1;  // day of month the daily feeds were last fetched
static String pinKey;      // "" = rotate; otherwise the card to keep on screen (survives restarts)
// batch 3 data
static std::vector<TriviaQ> trivia;
static size_t triviaIdx = 0;
static FactQuote factQuote;
static bool nextIsQuote = false;
static std::vector<ChartWeek> charts;
static size_t chartIdx = 0;
static std::vector<String> natDays, hnTitles;
static SurfReport surf;
static IssFix iss;
static GameResult flyW;
static uint32_t nextTrivia = 0, nextFact = 0, nextHn = 0, nextSurf = 0, nextIss = 0, nextFlyW = 0, nextDaily3 = 0;
static int daily3Day = -1;
// batch 4 data
static FptsBoard fpts;
static FantasyCache fantasyCache;  // netTask only
static std::map<String, WinProb> winProbs;                    // by game key
static std::map<String, std::vector<Highlight>> highlights;   // by game key (finals)
static GolfBoard golf;
static std::vector<NextGame> nextGames;
static std::vector<Flight> flights;
static uint32_t nextFpts = 0, nextExtras = 0, nextGolf = 0, nextNextGame = 0, nextFlights = 0;
// batch 5 data
static std::vector<Launch> launches;
static ParkWaits parkWaits;
static std::vector<StockQuote> stockQuotes;    // netTask's working copy is the same vector (under the lock)
static std::vector<NewsItem> newsItems;        // newest first, at most 10 (the "recent alerts" card)
static std::vector<NewsItem> newsQueue;        // banners waiting to drop down
static DriveTime drive;
static LottoDraw lotto;
static int pickemW = 0, pickemL = 0;           // season record (Preferences "pickem")
static uint32_t nextLaunch = 0, nextWaits = 0, nextStocks = 0, nextNews = 0, nextDrive = 0, nextLotto = 0;
static PetState pet;
static uint32_t petSavedAt = 0;
static bool petCheerPending = false;           // a favorite scored: the pet and the eyes get excited
static uint32_t excitedUntil = 0;              // millis()
static bool cardWantsShake = false, cardShaken = false;  // the fortune ball and party tools take shakes
static long sandTotal = 0;                     // running sand timer: total seconds (0 = none)
static time_t sandEnd = 0;
// some screens shouldn't be covered by a news banner (celebrations, menus, timers): hold them meanwhile
static int holdBanners = 0;
struct HoldBanners {
  HoldBanners() { holdBanners++; }
  ~HoldBanners() { holdBanners--; }
};
static bool cardWantsTaps = false, cardTapped = false;  // interactive screens take single taps themselves
static bool flatSleeping = false;
static uint32_t flatSince = 0;
static char orientKey = 'L';  // L landscape, F upside down, P portrait

static void savePin() {
  Preferences p;
  p.begin("screen", false);
  p.putString("pin", pinKey);
  p.end();
}

static void updateCardsJson() {
  JsonDocument d;
  d["pinned"] = pinKey;
  JsonArray a = d["cards"].to<JsonArray>();
  std::vector<String> seen;
  for (auto &c : rot.all()) {
    String k = pinKeyOf(c);
    if (CFG.has(seen, k)) continue;
    seen.push_back(k);
    JsonObject o = a.add<JsonObject>();
    o["k"] = k;
    o["l"] = cardLabel(c);
  }
  String out;
  serializeJson(d, out);
  webCardsJson = out;
  // pick'em: games not started yet (and today's), for the phone page
  JsonDocument pg;
  JsonArray ga = pg.to<JsonArray>();
  time_t now = time(nullptr);
  for (auto &kv : games)
    for (auto &g : kv.second) {
      if (g.state == 'f' && now - g.start > 86400) continue;
      JsonObject o = ga.add<JsonObject>();
      o["lg"] = g.league;
      o["id"] = g.id;
      o["a"] = g.away.abbr;
      o["h"] = g.home.abbr;
      o["t"] = (long)g.start;
      o["s"] = String(g.state);
      if (g.line.length()) o["ln"] = g.line;
    }
  String po;
  serializeJson(pg, po);
  webPickemJson = po;
}
static uint32_t nextUfc = 0;
static uint32_t nextWeather = 0, nextCalendar = 0, nextSpotify = 0;
static BusyMode busyMode = BUSY_AUTO;
static bool focusOn = false, onBreak = false;
static time_t focusEnd = 0;
static long focusTotal = 0;
static time_t lastChimeFor = 0;
static size_t todoIdx = 0;
static uint8_t wheelIdle = 0xFF;  // expander inputs at rest (captured at boot)
static uint32_t lastWheelPoll = 0;

// Thumbwheel via the PCA9557: returns 1 (up/down -> next), 2 (press -> favorites), 0 (nothing)
static int pollWheel() {
  if (!expanderFound() || millis() - lastWheelPoll < 30) return 0;
  lastWheelPoll = millis();
  const uint8_t mask = (1 << PCA_K1) | (1 << PCA_K2) | (1 << PCA_K3);
  uint8_t changed = (expanderRead() ^ wheelIdle) & mask;  // keys are active LOW
  if (!changed) return 0;
  delay(50);                                               // debounce: must still be pressed
  changed &= (expanderRead() ^ wheelIdle);
  if (!changed) return 0;
  int r = (changed & (1 << PCA_K2)) ? 2 : 1;
  while (((expanderRead() ^ wheelIdle) & changed)) {  // wait for release
    soundLoop();
    delay(10);
  }
  return r;
}

static time_t nowUtc() { return time(nullptr); }

static bool isNight(time_t t) {
  struct tm lt;
  localtime_r(&t, &lt);
  int h = lt.tm_hour, s = CFG.nightStart, e = CFG.nightEnd;
  return s > e ? (h >= s || h < e) : (h >= s && h < e);
}

// ------------------------------------------------------------------ waiting while staying responsive
static void startFocus(int minutes, bool brk);
static void stopFocus();

// Returns 1 = next card, 2 = press (focus timer), 3 = favorites toggle, 4 = state changed (redraw now),
// 5 = games (phone page or a shake), 0 = timed out
// news banners are held (they still collect in the alerts card) in quiet hours, during the night and while
// something that shouldn't be covered is up
static bool newsBannerOk() {
  if (!CFG.newsAlerts || otaBusy || holdBanners || focusOn || sandTotal) return false;
  time_t now = nowUtc();
  struct tm lt;
  localtime_r(&now, &lt);
  int h = lt.tm_hour, a = CFG.newsQuietStart, b = CFG.newsQuietEnd;
  bool quiet = a == b ? false : a < b ? (h >= a && h < b) : (h >= a || h < b);
  return !quiet;
}

static int pump(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    xSemaphoreGiveRecursive(dataMtx);  // let netTask hand over fresh data
    vTaskDelay(1);
    xSemaphoreTakeRecursive(dataMtx, portMAX_DELAY);
    if (otaBusy) {  // an automatic update is downloading in the background
      static uint32_t lastDraw = 0;
      if (millis() - lastDraw > 500) {
        lastDraw = millis();
        drawMessage("UPDATING", String((unsigned)(otaBytes / 1024)) + " KB", 0x40A0FF);
      }
      continue;
    }
    soundLoop();
    webLoop();
    motionPoll();
    if (motion().present) {
      const Motion &m = motion();
      // lying flat: screen off until it's picked up. Only once the sensor's mounting has been calibrated, so a
      // sensor mounted some other way round can never blank the screen while the clock is standing up.
      if (CFG.flatSleep || flatSleeping) {
        if (m.flat && m.calibrated && CFG.flatSleep) {
          if (!flatSince) flatSince = millis();
          if (!flatSleeping && millis() - flatSince > 2000) {
            flatSleeping = true;
            displaySetBrightness(0);
          }
        } else {
          flatSince = 0;
          if (flatSleeping) {
            flatSleeping = false;
            displaySetBrightness(isNight(nowUtc()) ? CFG.nightBrightness : CFG.brightness);
          }
        }
      }
      char ok = motionPortrait() ? 'P' : motionLandscapeRotation() == 2 ? 'F' : 'L';
      if (ok != orientKey) {  // turned: rebuild for the new shape
        orientKey = ok;
        return 4;
      }
      if (motionTakeShake()) {
        if (cardWantsShake) {
          cardShaken = true;  // the fortune ball / party tools use the shake themselves
        } else {
          webGameCmd = 99;  // shake: games menu
          return 5;
        }
      }
      if (CFG.accelTaps) {
        int tap = motionTakeTap();
        if (tap == 1) {
          if (cardWantsTaps) cardTapped = true;
          else return 1;  // tap: next card
        } else if (tap == 2) {
          return cardWantsTaps ? 1 : 6;  // double tap: next (on interactive screens) / the tap menu
        }
      }
    }
    if (CFG.clapControl && soundReady && micTakeDoubleClap()) return 1;  // two claps: next screen
    if (webWantsRestart) {
      delay(300);
      ESP.restart();
    }
    if (webTestSound.length()) {
      String s = webTestSound;
      webTestSound = "";
      int us = s.indexOf('_');
      if (us > 0) soundPlay(s.substring(us + 1), s.substring(0, us));
      else soundPlay(s);
    }
    if (webGameCmd != -2) {
      if (webGameCmd == -1) webGameCmd = -2;  // "Stop" with no game running: nothing to do
      else return 5;
    }
    if (webNext) {
      webNext = false;
      return 1;
    }
    if (webToggleFav) {
      webToggleFav = false;
      return 3;
    }
    if (webBusyMode >= 0) {
      busyMode = (BusyMode)(int)webBusyMode;
      webBusyMode = -1;
      return 4;
    }
    if (webSandCmd >= 0) {  // phone page: start (seconds) or stop (0) the sand timer
      int sec = webSandCmd;
      webSandCmd = -1;
      sandTotal = sec;
      sandEnd = sec > 0 ? nowUtc() + sec : 0;
      return 4;
    }
    if (!newsQueue.empty() && newsBannerOk()) return 7;  // a news alert drops down over the screen
    if (webFocusCmd >= 0) {
      int m = webFocusCmd;
      webFocusCmd = -1;
      if (m == 0) stopFocus();
      else startFocus(m, false);
      return 4;
    }
    if (webMsLogin) {
      webMsLogin = false;
      msLoginRequested = true;  // netTask asks Microsoft for a code
      return 4;
    }
    if (webDeskChanged) {
      webDeskChanged = false;
      return 4;
    }
    if (webPinChanged) {
      webPinChanged = false;
      if (webPinRequest == "__current__") {
        const Card *c = rot.current();
        pinKey = c ? pinKeyOf(*c) : "";
      } else {
        pinKey = webPinRequest;
      }
      savePin();
      return 4;
    }
    if (int w = pollWheel()) return w;
    if (digitalRead(BTN_BOOT) == LOW) {
      uint32_t p0 = millis();
      while (digitalRead(BTN_BOOT) == LOW) {
        soundLoop();
        delay(5);
      }
      return (millis() - p0 > 900) ? 2 : 1;
    }
    delay(2);
  }
  return 0;
}

// ------------------------------------------------------------------ data
static uint32_t intervalFor(const std::vector<Game> &lg, time_t now) {
  for (auto &g : lg) if (g.state == 'i') return CFG.pollLive;
  for (auto &g : lg) if (g.state == 'p' && g.start - now > 0 && g.start - now < 3600) return CFG.pollSoon;
  return lg.empty() ? CFG.pollOff : CFG.pollIdle;
}

static bool refreshData() {
  bool changed = false;
  time_t now = nowUtc();
  for (auto &lg : CFG.leagues) {
    if ((int32_t)(millis() - nextFetch[lg]) < 0) continue;
    std::vector<Game> fresh;
    if (fetchScoreboard(lg, fresh)) {
      Lock lock;
      // favorite team just went final with a win -> victory celebration
      for (auto &ng : fresh) {
        if (ng.state != 'f' || !isFavoriteGame(ng)) continue;
        for (auto &og : games[lg]) {
          if (og.id != ng.id || og.state != 'i') continue;
          bool homeFav = CFG.isFavorite(lg, ng.home.abbr);
          int mine = homeFav ? ng.home.score : ng.away.score, theirs = homeFav ? ng.away.score : ng.home.score;
          if (mine > theirs) {
            ScoreEvent w;
            w.game = ng;
            w.home = homeFav;
            w.delta = 0;
            w.label = "WIN!";
            w.sound = "win";
            pendingCelebrations.push_back(w);
          }
        }
      }
      games[lg] = fresh;
      changed = true;
      for (auto &ev : watch.update(lg, fresh))
        if (shouldCelebrate(ev)) pendingCelebrations.push_back(ev);
    }
    nextFetch[lg] = millis() + intervalFor(games[lg], now) * 1000UL;
    soundLoop();
  }

  if (!CFG.fantasy.empty() && (int32_t)(millis() - nextFantasy) >= 0) {
    static std::map<String, std::map<String, StatLine>> finalLines;
    std::map<String, std::map<String, StatLine>> trackedFinals;
    std::vector<Card> cards;
    for (auto &lg : CFG.leagues)
      for (auto &g : games[lg]) {
        if (g.state == 'p') continue;
        std::vector<FantasyPlayer> want;
        for (auto &p : CFG.fantasy)
          if (p.league == g.league && (p.team == g.away.abbr || p.team == g.home.abbr)) want.push_back(p);
        if (want.empty()) continue;
        String gameKey = g.league + ":" + g.id;
        auto cached = finalLines.find(gameKey);
        std::map<String, StatLine> liveLines;
        const std::map<String, StatLine> *lines = &liveLines;
        if (g.state == 'f' && cached != finalLines.end()) {
          lines = &trackedFinals.emplace(gameKey, std::move(cached->second)).first->second;
        } else {
          if (!fetchBoxLines(g, want, liveLines)) continue;
          if (g.state == 'f' && !liveLines.empty())
            lines = &trackedFinals.emplace(gameKey, std::move(liveLines)).first->second;
        }
        for (const auto &kv : *lines) {
          Card c;
          c.kind = CARD_FANTASY;
          c.key = "fan:" + kv.first;
          c.live = g.state == 'i';
          c.pri = PRI_FANTASY + (c.live ? 5 : 0);
          c.dwell = 6;
          int sp = kv.first.lastIndexOf(' ');
          c.title = sp >= 0 ? kv.first.substring(sp + 1) : kv.first;
          c.title.toUpperCase();
          c.text = kv.second.line + (kv.second.slash.length() ? "  " + kv.second.slash : "");
          cards.push_back(c);
        }
      }
    finalLines.swap(trackedFinals);
    {
      Lock lock;
      fantasyCards = std::move(cards);
    }
    nextFantasy = millis() + CFG.pollFantasy * 1000UL;
    changed = true;
  }

  if (weatherConfigured() && (int32_t)(millis() - nextWeather) >= 0) {
    Weather w;
    if (fetchWeather(w)) {
      Lock lock;
      wx = w;
    }
    nextWeather = millis() + (wx.ok ? CFG.pollWeather : 120) * 1000UL;
  }

  if (msSignedIn() && (int32_t)(millis() - nextCalendar) >= 0) {
    std::vector<Meeting> m;
    std::vector<String> f;
    bool okM, okF;
    String taskToken;
    {
      Lock lock;  // msgraph keeps its token in shared state; calendar calls are quick
      okM = msFetchCalendar(now, m);
      okF = msFetchFlagged(f);
      if (okM) meetings = m;
      if (okF) flagged = f;
      if (CFG.msTodo) taskToken = msTaskToken();
    }
    if (taskToken.length()) {
      std::vector<String> tasks;
      if (msFetchTasks(taskToken, tasks)) {
        Lock lock;
        if (msTaskTokenCurrent(taskToken)) todoSetMicrosoft(std::move(tasks));
      }
    }
    nextCalendar = millis() + CFG.pollCalendar * 1000UL;
    changed = true;
  }

  // standings: every 6 hours per league, in season only
  for (auto &lg : CFG.leagues) {
    if (!(CFG.cardStandings || CFG.cardPlayoff) || !inSeason(lg, now) || (int32_t)(millis() - nextStandings[lg]) < 0) continue;
    std::vector<StandGroup> g;
    bool ok = fetchStandings(lg, g);
    if (ok) {
      Lock lock;
      standings[lg] = g;
    }
    nextStandings[lg] = millis() + (ok ? 6 * 3600 : 900) * 1000UL;
    changed = true;
  }

  // Teams presence for the busy light, every minute
  if (CFG.teamsPresence && msSignedIn() && (int32_t)(millis() - nextPresence) >= 0) {
    String a, act;
    Lock lock;
    if (msFetchPresence(a, act)) {
      presAvail = a;
      presActivity = act;
    }
    nextPresence = millis() + 60000UL;
  }

  // daily feeds: on this day, word of the day (refetch when the date changes)
  struct tm lt;
  localtime_r(&now, &lt);
  static uint32_t nextFeedsTry = 0;
  if (lt.tm_mday != feedsDay && now > 1700000000 && (int32_t)(millis() - nextFeedsTry) >= 0) {
    bool ok = true;
    if (CFG.cardHistory) {
      std::vector<HistoryEvent> h;
      if (fetchHistory(lt.tm_mon + 1, lt.tm_mday, CFG.historyGentle, h)) {
        Lock lock;
        history = h;
      } else ok = false;
    }
    if (CFG.cardWord) {
      WordOfDay w;
      if (fetchWord(w)) {
        Lock lock;
        wotd = w;
      } else ok = false;
    }
    if (ok) feedsDay = lt.tm_mday;
    else nextFeedsTry = millis() + 10 * 60000UL;  // a feed is down: try again in 10 minutes, not every card
    changed = true;
  }
  if (CFG.cardJoke && (int32_t)(millis() - nextJoke) >= 0) {
    String j;
    bool ok = fetchJoke(j);
    if (ok) {
      Lock lock;
      joke = j;
    }
    nextJoke = millis() + (ok ? 1800 : 300) * 1000UL;
  }

  // ---- batch 3 feeds
  if (CFG.cardTrivia && (int32_t)(millis() - nextTrivia) >= 0 && triviaIdx >= trivia.size()) {
    std::vector<TriviaQ> t;
    bool ok = fetchTrivia(t);
    if (ok) {
      Lock lock;
      trivia = t;
      triviaIdx = 0;
    }
    nextTrivia = millis() + (ok ? 600 : 300) * 1000UL;
    changed |= ok;
  }
  if (CFG.cardFacts && (int32_t)(millis() - nextFact) >= 0) {
    FactQuote f;
    bool ok = nextIsQuote ? fetchQuote(f) : fetchFact(f);
    if (ok) {
      Lock lock;
      factQuote = f;
    }
    nextIsQuote = !nextIsQuote;
    nextFact = millis() + (ok ? 900 : 300) * 1000UL;
    changed |= ok;
  }
  if (lt.tm_mday != daily3Day && now > 1700000000 && (int32_t)(millis() - nextDaily3) >= 0) {
    bool ok = true;
    if (CFG.cardBillboard) {
      std::vector<ChartWeek> cw;
      for (int y : CFG.billboardYears) {
        ChartWeek w;
        if (fetchChart(now, y, w)) cw.push_back(w);
      }
      if (!cw.empty()) {
        Lock lock;
        charts = cw;
      } else ok = false;
    }
    if (CFG.cardNationalDay) {
      std::vector<String> nd;
      if (fetchNationalDays(nd)) {
        Lock lock;
        natDays = nd;
      } else ok = false;
    }
    if (ok) daily3Day = lt.tm_mday;
    else nextDaily3 = millis() + 15 * 60000UL;
    changed = true;
  }
  if (CFG.cardHackerNews && (int32_t)(millis() - nextHn) >= 0) {
    std::vector<String> t;
    bool ok = fetchHackerNews(t, 5);
    if (ok) {
      Lock lock;
      hnTitles = t;
    }
    nextHn = millis() + (ok ? 1800 : 300) * 1000UL;
    changed |= ok;
  }
  if (CFG.cardSurf && (int32_t)(millis() - nextSurf) >= 0) {
    SurfReport r;
    bool ok = fetchSurf(CFG.surfLat, CFG.surfLon, r);
    if (ok) {
      Lock lock;
      surf = r;
    }
    nextSurf = millis() + (ok ? 1800 : 300) * 1000UL;
    changed |= ok;
  }
  if (CFG.cardIss && (int32_t)(millis() - nextIss) >= 0) {
    IssFix f = iss;
    bool ok = fetchIss(f);
    if (ok) {
      Lock lock;
      iss = f;
    }
    nextIss = millis() + (ok ? 60 : 180) * 1000UL;
  }
  if (CFG.cardFlyW && (int32_t)(millis() - nextFlyW) >= 0) {
    String team;
    for (auto &f : CFG.favorites)
      if (f.first == "mlb") {
        team = f.second;
        break;
      }
    bool ok = false;
    if (team.length()) {
      GameResult r;
      ok = fetchLastResult(team, now, r);
      if (ok) {
        Lock lock;
        flyW = r;
      }
    }
    nextFlyW = millis() + (ok ? 1200 : 600) * 1000UL;
    changed |= ok;
  }

  // ---- batch 4 feeds
  if (CFG.cardFpts && !CFG.fptsPlayers.empty() && (int32_t)(millis() - nextFpts) >= 0) {
    std::vector<Game> nfl;
    bool ok;
    if (CFG.fptsWeek == 0 && CFG.has(CFG.leagues, "nfl")) {
      nfl = games["nfl"];  // only this task writes games, so reading it here needs no lock
      ok = !nfl.empty();
    } else {
      ok = fetchScoreboard("nfl", nfl, CFG.fptsWeek ? "week=" + String(CFG.fptsWeek) + "&seasontype=2" : String());
    }
    bool live = false;
    if (ok) {
      FptsBoard b = fantasyBuild(nfl, fantasyFetchSummary, fantasyCache);
      live = b.anyLive;
      Lock lock;
      fpts = b;
    }
    nextFpts = millis() + (live ? CFG.pollFantasy : ok ? 600 : 120) * 1000UL;
    changed = true;
  }
  if ((CFG.cardWinProb || CFG.cardHighlights) && CFG.sports != "off" && (int32_t)(millis() - nextExtras) >= 0) {
    // favorite games: the win-probability line while live, and once more (with top performers) when final
    std::map<String, WinProb> wp;
    std::map<String, std::vector<Highlight>> hl;
    {
      Lock lock;
      wp = winProbs;
      hl = highlights;
    }
    std::vector<String> keep;
    for (auto &lg : CFG.leagues)
      for (auto &g : games[lg]) {
        if (!isFavoriteGame(g) || g.state == 'p' || priorityOf(g, now) <= 0) continue;
        String k = g.key();
        keep.push_back(k);
        bool finalDone = g.state == 'f' && hl.count(k);
        if (finalDone) continue;
        WinProb w;
        std::vector<Highlight> h;
        if (fetchGameExtras(g, g.state == 'f', w, h)) {
          wp[k] = w;
          if (g.state == 'f') hl[k] = h;
        }
      }
    for (auto it = wp.begin(); it != wp.end();) it = CFG.has(keep, it->first) ? std::next(it) : wp.erase(it);
    for (auto it = hl.begin(); it != hl.end();) it = CFG.has(keep, it->first) ? std::next(it) : hl.erase(it);
    {
      Lock lock;
      winProbs = wp;
      highlights = hl;
    }
    nextExtras = millis() + std::max(30, CFG.pollLive) * 1000UL;
    changed = true;
  }
  if (CFG.cardGolf && CFG.sports != "off" && (int32_t)(millis() - nextGolf) >= 0) {
    GolfBoard gb;
    bool ok = fetchGolf(CFG.golfTour, gb);
    if (ok) {
      Lock lock;
      golf = gb;
    }
    nextGolf = millis() + (ok ? (gb.state == 'i' ? 180 : 1800) : 600) * 1000UL;
    changed |= ok;
  }
  if (CFG.cardNextGame && CFG.sports != "off" && (int32_t)(millis() - nextNextGame) >= 0) {
    std::vector<NextGame> ng;
    bool ok = true;
    for (auto &f : CFG.favorites) {
      if (!CFG.has(CFG.leagues, f.first)) continue;
      NextGame n;
      if (!fetchNextGame(f.first, f.second, now, n)) ok = false;
      else if (n.ok) ng.push_back(n);
    }
    {
      Lock lock;
      nextGames = ng;
    }
    nextNextGame = millis() + (ok ? 3 * 3600 : 900) * 1000UL;
    changed = true;
  }
  if (CFG.cardFlights && (CFG.lat != 0 || CFG.lon != 0) && (int32_t)(millis() - nextFlights) >= 0) {
    std::vector<Flight> f;
    bool ok = fetchFlights(CFG.lat, CFG.lon, CFG.flightRadiusKm, f);
    if (ok) {
      Lock lock;
      flights = f;
    }
    nextFlights = millis() + (ok ? 300 : 900) * 1000UL;  // OpenSky allows ~400 anonymous calls a day
    changed |= ok;
  }

  // ---- batch 5 feeds
  struct tm lt5;
  localtime_r(&now, &lt5);
  if (CFG.cardLaunch && (int32_t)(millis() - nextLaunch) >= 0) {
    std::vector<Launch> l;
    bool ok = fetchLaunches(l);
    if (ok) {
      Lock lock;
      launches = l;
    }
    bool soon = ok && !l.empty() && l[0].net - now < 3600 && l[0].net - now > -1800;
    nextLaunch = millis() + (soon ? 300 : ok ? 1800 : 900) * 1000UL;  // the free API allows 15 calls an hour
    changed |= ok;
  }
  if (CFG.cardWaits && CFG.parkId.length() && (int32_t)(millis() - nextWaits) >= 0) {
    ParkWaits p;
    bool ok = fetchParkWaits(CFG.parkId, CFG.parkName, p);
    if (ok) {
      Lock lock;
      parkWaits = p;
    }
    nextWaits = millis() + (ok ? (p.rides.empty() ? 1800 : 300) : 900) * 1000UL;
    changed |= ok;
  }
  if (CFG.cardStocks && !CFG.stocks.empty() && (int32_t)(millis() - nextStocks) >= 0) {
    std::vector<StockQuote> q;
    {
      Lock lock;
      q = stockQuotes;
    }
    std::vector<StockQuote> out;  // follow the phone page's list (and its order), keeping each one's history
    for (auto &sym : CFG.stocks) {
      StockQuote one;
      one.symbol = sym;
      for (auto &x : q)
        if (x.symbol == sym) one = x;
      stockRollover(one, now);
      fetchStock(sym, one, now);
      out.push_back(one);
    }
    {
      Lock lock;
      stockQuotes = out;
    }
    bool weekend = lt5.tm_wday == 0 || lt5.tm_wday == 6;
    nextStocks = millis() + (weekend ? 1800 : 120) * 1000UL;
    changed = true;
  }
  if ((CFG.cardNews || CFG.newsAlerts) && (int32_t)(millis() - nextNews) >= 0) {
    static std::vector<String> seen;  // article ids already handled
    static bool primed = false;       // the first download only fills the list: no banners for old news
    NewsWatch w;
    if (CFG.newsFavorites) w.teams = CFG.favorites;
    if (CFG.newsFantasy)
      for (auto &p : CFG.fptsPlayers) {
        if (p.pos == "DST") w.teams.push_back({"nfl", p.team});
        else {
          if (p.id.length()) w.athleteIds.push_back(p.id);
          w.playerNames.push_back(p.name);
        }
      }
    w.keywords = CFG.newsKeywords;
    std::vector<NewsItem> found;
    bool ok = true;
    for (auto &lg : CFG.newsLeagues) {
      std::vector<NewsItem> n;
      if (fetchNews(lg, w, n)) found.insert(found.end(), n.begin(), n.end());
      else ok = false;
    }
    std::sort(found.begin(), found.end(), [](const NewsItem &a, const NewsItem &b) { return a.published > b.published; });
    std::vector<NewsItem> fresh;
    for (auto &n : found) {
      if (CFG.has(seen, n.id)) continue;
      seen.push_back(n.id);
      if (primed && now - n.published < 6 * 3600) fresh.push_back(n);
      else n.read = true;
      if (!primed) fresh.push_back(n);
    }
    if (seen.size() > 200) seen.erase(seen.begin(), seen.begin() + 100);
    if (!fresh.empty()) {
      Lock lock;
      for (auto it = fresh.rbegin(); it != fresh.rend(); ++it) {
        newsItems.insert(newsItems.begin(), *it);
        if (primed && CFG.newsAlerts) newsQueue.push_back(*it);
      }
      if (newsItems.size() > 10) newsItems.resize(10);
      if (newsQueue.size() > 5) newsQueue.erase(newsQueue.begin(), newsQueue.end() - 5);  // keep the latest
    }
    primed |= ok;
    nextNews = millis() + (ok ? 180 : 600) * 1000UL;
    changed |= !fresh.empty();
  }
  if (CFG.cardDrive && CFG.tomtomKey.length() && (CFG.lat != 0 || CFG.lon != 0) &&
      (int32_t)(millis() - nextDrive) >= 0) {
    bool ok = false;
    if (CFG.driveLat == 0 && CFG.driveLon == 0 && CFG.driveAddress.length()) {
      float la, lo;
      if (geocodeTomTom(CFG.tomtomKey, CFG.driveAddress, la, lo)) {
        Lock lock;
        CFG.driveLat = la;
        CFG.driveLon = lo;
      }
    }
    if (CFG.driveLat != 0 || CFG.driveLon != 0) {
      DriveTime d;
      ok = fetchDriveTime(CFG.tomtomKey, CFG.lat, CFG.lon, CFG.driveLat, CFG.driveLon, d);
      if (ok) {
        d.label = CFG.driveLabel;
        d.at = now;
        Lock lock;
        drive = d;
      }
    }
    bool window = lt5.tm_wday >= 1 && lt5.tm_wday <= 5 && lt5.tm_hour >= CFG.driveFrom - 1 && lt5.tm_hour < CFG.driveTo;
    nextDrive = millis() + (window ? 300 : 1800) * 1000UL;  // free tier: 2,500 calls a day
    changed |= ok;
  }
  if (CFG.cardLotto && (int32_t)(millis() - nextLotto) >= 0) {
    LottoDraw d;
    bool ok = fetchLotto(d, now);
    if (ok) {
      Lock lock;
      lotto = d;
    }
    // every 6 hours, and soon after a draw so the new numbers show up the same night
    long untilDraw = ok ? (long)(d.nextDraw - now) : 0;
    long wait = ok ? (untilDraw > 0 && untilDraw < 6 * 3600 ? untilDraw + 3600 : 6 * 3600) : 1800;
    nextLotto = millis() + wait * 1000UL;
    changed |= ok;
  }
  if (CFG.cardPickem && !CFG.pickem.empty()) {  // score the picks from the scoreboards already downloaded
    std::vector<PickemPick> picks;
    {
      Lock lock;
      picks = CFG.pickem;
    }
    if (pickemScore(picks, games)) {
      Lock lock;
      // season record: each decided game counted once (ids kept in Preferences)
      Preferences pr;
      pr.begin("pickem", false);
      String log = pr.getString("log", "");
      for (auto &p : picks) {
        if (p.result == ' ') continue;
        String tag = p.league + ":" + p.gameId + ";";
        if (log.indexOf(tag) >= 0) continue;
        log += tag;
        if (p.result == 'W') pickemW++;
        else if (p.result == 'L') pickemL++;
      }
      if (log.length() > 3500) log = log.substring(log.length() - 3000);
      pr.putString("log", log);
      pr.putInt("w", pickemW);
      pr.putInt("l", pickemL);
      pr.end();
      CFG.pickem = picks;
      changed = true;
    }
  }

  if (CFG.ufc && (int32_t)(millis() - nextUfc) >= 0) {
    std::vector<FightEvent> u;
    bool ok = fetchUfc(u);
    if (ok) {
      Lock lock;
      ufcEvents = u;
    }
    nextUfc = millis() + (ok ? 3 * 3600 : 600) * 1000UL;
    changed = true;
  }

  if (spotifyConfigured() && (int32_t)(millis() - nextSpotify) >= 0) {
    NowPlaying n = np;
    bool was = n.playing;
    spotifyNowPlaying(n);
    {
      Lock lock;
      np = n;
    }
    nextSpotify = millis() + (n.playing ? 10 : 30) * 1000UL;
    changed |= was != n.playing;
  }

  if (yahooConfigured() && (int32_t)(millis() - nextYahoo) >= 0) {
    Card c;
    bool ok = yahooMatchup(now, c);
    Lock lock;
    haveYahoo = ok;
    if (ok) yahooCard = c;
    nextYahoo = millis() + CFG.pollYahoo * 1000UL;
    changed = true;
  }
  return changed;
}

static NowPlaying currentMusic() { return np; }  // Spotify

static std::vector<String> allTodos() {
  std::vector<String> t = todoOpen();
  for (auto &f : flagged) t.push_back("MAIL: " + f);
  return t;
}

static void rebuild() {
  time_t now = nowUtc();
  std::vector<Game> all;
  if (CFG.sports != "off")
    for (auto &lg : CFG.leagues) all.insert(all.end(), games[lg].begin(), games[lg].end());
  std::vector<Card> extra;
  if (CFG.sports != "off") {
    extra = fantasyCards;
    if (haveYahoo) extra.push_back(yahooCard);
  }
  std::vector<Card> desk = buildDeskCards(meetings, allTodos(), currentMusic(), now, wx);
  extra.insert(extra.end(), desk.begin(), desk.end());
  Card ufc;
  if (CFG.sports != "off" && ufcCard(ufcEvents, now, ufc)) extra.push_back(ufc);
  if (CFG.sports != "off")
    for (auto &kv : standings) {
      auto st = standingsCards(kv.second, now);
      extra.insert(extra.end(), st.begin(), st.end());
    }
  auto ambient = [&](CardKind k, const char *key, int dwell, int pri) {
    Card c;
    c.kind = k;
    c.key = key;
    c.dwell = dwell;
    c.pri = pri;
    extra.push_back(c);
  };
  if (CFG.cardHistory && !history.empty()) ambient(CARD_HISTORY, "history", 10, PRI_AMBIENT + 2);
  if (CFG.cardWord && wotd.ok) ambient(CARD_WORD, "word", 9, PRI_AMBIENT + 2);
  if (CFG.cardJoke && joke.length()) ambient(CARD_JOKE, "joke", 9, PRI_AMBIENT + 1);
  if (CFG.cardTetris) ambient(CARD_TETRIS, "tetris", 12, PRI_AMBIENT);
  if (CFG.cardRain) ambient(CARD_RAIN, "rain", 8, PRI_AMBIENT);
  if (CFG.cardFire) ambient(CARD_FIRE, "fire", 10, PRI_AMBIENT);
  if (CFG.cardBounce) ambient(CARD_BOUNCE, "bounce", 10, PRI_AMBIENT);
  if (CFG.cardSnake) ambient(CARD_SNAKE, "snake", 12, PRI_AMBIENT);
  if (CFG.cardBreakout) ambient(CARD_BREAKOUT, "breakout", 12, PRI_AMBIENT);
  if (CFG.cardImages && imageCount() > 0) ambient(CARD_IMAGES, "images", 8, PRI_AMBIENT + 1);
  if (CFG.cardNixie && CFG.clockStyle != "nixie") ambient(CARD_NIXIE, "nixie", 10, PRI_AMBIENT);
  if (CFG.cardBigClock && CFG.clockStyle != "big") ambient(CARD_BIGCLOCK, "bigclock", 10, PRI_AMBIENT);
  if (CFG.cardWordClock && CFG.clockStyle != "words") ambient(CARD_WORDCLOCK, "wordclock", 10, PRI_AMBIENT);
  if (CFG.cardAquarium) ambient(CARD_AQUARIUM, "aquarium", 15, PRI_AMBIENT);
  if (CFG.cardPendulum) ambient(CARD_PENDULUM, "pendulum", 15, PRI_AMBIENT);
  if (CFG.cardCradle) ambient(CARD_CRADLE, "cradle", 15, PRI_AMBIENT);
  if (CFG.cardWater) ambient(CARD_WATER, "water", 15, PRI_AMBIENT);
  if (CFG.cardTrivia && triviaIdx < trivia.size()) ambient(CARD_TRIVIA, "trivia", 14, PRI_AMBIENT + 2);
  if (CFG.cardFacts && factQuote.text.length()) ambient(CARD_FACT, "fact", 10, PRI_AMBIENT + 1);
  if (CFG.cardBillboard && !charts.empty()) ambient(CARD_BILLBOARD, "billboard", 10, PRI_AMBIENT + 1);
  if (CFG.cardNationalDay && !natDays.empty()) ambient(CARD_NATDAY, "natday", 8, PRI_AMBIENT + 2);
  if (CFG.cardHackerNews && !hnTitles.empty()) ambient(CARD_HN, "hn", 12, PRI_AMBIENT + 1);
  if (CFG.cardSurf && surf.ok) ambient(CARD_SURF, "surf", 8, PRI_AMBIENT + 1);
  if (CFG.cardIss && iss.ok) ambient(CARD_ISS, "iss", 10, PRI_AMBIENT + 1);
  if (CFG.cardPlanets && CFG.lat != 0) ambient(CARD_PLANETS, "planets", 10, PRI_AMBIENT + 1);
  if (CFG.cardFlyW && flyW.ok && flyW.won && now - flyW.when < 22 * 3600) ambient(CARD_FLYW, "flyw", 10, PRI_FAV_FINAL - 1);
  // ---- batch 4
  if (CFG.cardBinary) ambient(CARD_BINARY, "binary", 10, PRI_AMBIENT);
  if (CFG.cardSunArc) ambient(CARD_SUNARC, "sunarc", 10, PRI_AMBIENT);
  if (CFG.cardCity) ambient(CARD_CITY, "city", 12, PRI_AMBIENT);
  if (CFG.cardWorld && !CFG.worldClocks.empty()) ambient(CARD_WORLD, "world", 8, PRI_AMBIENT + 1);
  if (CFG.cardSand) ambient(CARD_SAND, "sand", 15, PRI_AMBIENT);
  if (CFG.cardLife) ambient(CARD_LIFE, "life", 15, PRI_AMBIENT);
  if (CFG.cardVisualizer && soundReady && micPresent()) ambient(CARD_VISUALIZER, "visualizer", 15, PRI_AMBIENT);
  if (CFG.cardFlights && !flights.empty()) ambient(CARD_FLIGHTS, "planes", 10, PRI_AMBIENT + 1);
  if (CFG.cardFpts && fpts.ok) {
    Card c;
    c.kind = CARD_FPTS;
    c.key = "fpts";
    c.dwell = 12;
    c.live = fpts.anyLive;
    c.pri = fpts.anyLive ? PRI_FAV_LIVE : fpts.anyStarted ? PRI_FANTASY + 5 : PRI_AMBIENT + 2;
    extra.push_back(c);
  }
  // ---- batch 5
  if (CFG.cardFlap) ambient(CARD_FLAP, "flap", 12, PRI_AMBIENT);
  if (CFG.cardDials) ambient(CARD_DIALS, "dials", 10, PRI_AMBIENT);
  if (CFG.cardTerminal) ambient(CARD_TERMINAL, "terminal", 12, PRI_AMBIENT);
  if (CFG.cardSlots) ambient(CARD_SLOTS, "slots", 10, PRI_AMBIENT);
  if (CFG.cardWeeks && CFG.birthYear > 1900) ambient(CARD_WEEKS, "weeks", 8, PRI_AMBIENT);
  if (CFG.cardEyes) ambient(CARD_EYES, "eyes", 12, PRI_AMBIENT);
  if (CFG.cardEightBall) ambient(CARD_EIGHTBALL, "fortune", 10, PRI_AMBIENT);
  if (CFG.cardParty) ambient(CARD_PARTY, "party", 10, PRI_AMBIENT);
  if (CFG.cardMaze) ambient(CARD_MAZE, "maze", 15, PRI_AMBIENT);
  if (CFG.cardPet) ambient(CARD_PET, "pet", 12, PRI_AMBIENT + 1);
  if (CFG.cardLaunch && !launches.empty() && launches[0].net - now < 7 * 86400L) {
    Card c;
    c.kind = CARD_LAUNCH;
    c.key = "launch";
    c.title = launches[0].rocket;
    c.dwell = 10;
    long t = (long)(launches[0].net - now);
    c.pri = t < 600 && t > -300 ? PRI_FAV_LIVE : launches[0].vandenberg && t < 3600 && t > 0 ? PRI_FAV_PRE
                                                                                           : PRI_AMBIENT + 2;
    c.live = t < 600 && t > -300;
    extra.push_back(c);
  }
  if (CFG.cardWaits && parkWaits.ok && !parkWaits.rides.empty()) {
    Card c;
    c.kind = CARD_WAITS;
    c.key = "waits";
    c.dwell = 4 * std::max(1, parkPages(parkWaits));
    c.pri = PRI_AMBIENT + 2;
    extra.push_back(c);
  }
  if (CFG.cardStocks)
    for (auto &q : stockQuotes) {
      if (!q.ok) continue;
      Card c;
      c.kind = CARD_STOCK;
      c.key = "stock:" + q.symbol;
      c.title = q.symbol;
      c.text = q.symbol;
      c.dwell = 6;
      c.pri = PRI_AMBIENT + 2;
      extra.push_back(c);
    }
  if (CFG.cardNews && !newsItems.empty()) ambient(CARD_NEWS, "news", 4 * std::min<int>(5, newsItems.size()), PRI_AMBIENT + 3);
  if (CFG.cardDrive && drive.ok && now - drive.at < 3600) {
    struct tm lt;
    localtime_r(&now, &lt);
    bool window = lt.tm_wday >= 1 && lt.tm_wday <= 5 && lt.tm_hour >= CFG.driveFrom && lt.tm_hour < CFG.driveTo;
    ambient(CARD_DRIVE, "drive", 8, window ? PRI_MEETING_LATER : PRI_AMBIENT + 1);
  }
  if (CFG.cardLotto && lotto.ok) ambient(CARD_LOTTO, "powerball", 8, PRI_AMBIENT + 1);
  if (CFG.cardPickem && CFG.sports != "off" && !CFG.pickem.empty()) {
    bool live = false;
    for (auto &p : CFG.pickem)
      for (auto &g : games[p.league])
        if (g.id == p.gameId && g.state == 'i') live = true;
    ambient(CARD_PICKEM, "pickem", 4 * std::max(1, pickemPages(CFG.pickem)), live ? PRI_ALL_LIVE + 5 : PRI_FANTASY);
  }
  if (CFG.sports != "off") {
    if (CFG.cardLastPlay)  // live favorite games with a last play to show
      for (auto &lg : CFG.leagues)
        for (auto &g : games[lg]) {
          if (g.state != 'i' || !isFavoriteGame(g) || !g.lastPlay.length()) continue;
          Card c;
          c.kind = CARD_LASTPLAY;
          c.key = "play:" + g.key();
          c.text = g.key();
          c.title = g.away.abbr + " @ " + g.home.abbr;
          c.dwell = 8;
          c.live = true;
          c.pri = PRI_FAV_LIVE;
          extra.push_back(c);
        }
    if (CFG.cardPlayoff)  // each favorite's group in the standings
      for (auto &f : CFG.favorites) {
        auto it = standings.find(f.first);
        if (it == standings.end()) continue;
        for (auto &grp : it->second) {
          bool in = false;
          for (auto &r : grp.rows) in |= r.abbr == f.second;
          if (!in) continue;
          Card c;
          c.kind = CARD_PLAYOFF;
          c.key = "race:" + f.first + ":" + f.second;
          c.text = f.first + ":" + f.second;
          c.title = f.second;
          c.dwell = 8;
          c.pri = PRI_AMBIENT + 3;
          extra.push_back(c);
          break;
        }
      }
  }
  std::vector<String> nextIds;  // favorites' pre-game cards are replaced by the richer "next game" card
  if (CFG.sports != "off") {
    if (CFG.cardWinProb)
      for (auto &kv : winProbs) {
        if (!kv.second.ok) continue;
        Card c;
        c.kind = CARD_WINPROB;
        c.key = "wp:" + kv.first;
        c.text = kv.first;
        c.title = kv.second.game.away.abbr + " @ " + kv.second.game.home.abbr;
        c.dwell = 8;
        c.pri = kv.second.game.state == 'i' ? PRI_FAV_LIVE : PRI_FAV_FINAL - 1;
        extra.push_back(c);
      }
    if (CFG.cardHighlights)
      for (auto &kv : highlights) {
        if (kv.second.empty()) continue;
        Card c;
        c.kind = CARD_HIGHLIGHT;
        c.key = "hl:" + kv.first;
        c.text = kv.first;
        c.title = kv.second[0].team;
        c.dwell = 4 * (int)kv.second.size();
        c.pri = PRI_FAV_FINAL - 2;
        extra.push_back(c);
      }
    if (CFG.cardGolf && golf.ok && (golf.state == 'i' || (golf.state == 'f' && now - golf.start < 5 * 86400))) {
      Card c;
      c.kind = CARD_GOLF;
      c.key = "golf";
      c.dwell = 3 * golfPages(golf);
      c.pri = golf.state == 'i' ? PRI_ALL_LIVE : PRI_AMBIENT + 3;
      extra.push_back(c);
    }
    if (CFG.cardNextGame)
      for (auto &n : nextGames) {
        if (n.start + 4 * 3600 < now) continue;
        Card c;
        c.kind = CARD_NEXTGAME;
        c.key = "next:" + n.league + ":" + n.us;
        c.text = n.league + ":" + n.us;
        c.title = n.us;
        c.dwell = 8;
        c.pri = n.start - now < CFG.upcomingHours * 3600L ? PRI_FAV_PRE : PRI_AMBIENT + 3;
        extra.push_back(c);
        nextIds.push_back(n.league + ":" + n.id);
      }
  }
  if (motionPortrait()) {  // standing on its end: only the tall screens (and water, which follows gravity)
    std::vector<Card> p;
    auto add = [&](CardKind k, const char *key, int dwell) {
      Card c;
      c.kind = k;
      c.key = key;
      c.dwell = dwell;
      c.pri = PRI_AMBIENT;
      p.push_back(c);
    };
    if (CFG.cardPClock) add(CARD_PCLOCK, "pclock", CFG.clockDwell);
    if (CFG.cardBeer) add(CARD_BEER, "beer", 20);
    if (CFG.cardHourglass) add(CARD_HOURGLASS, "hourglass", 20);
    if (CFG.cardLava) add(CARD_LAVA, "lava", 20);
    if (CFG.cardWater) add(CARD_WATER, "water", 20);
    if (p.empty()) add(CARD_PCLOCK, "pclock", CFG.clockDwell);
    rot.update(p);
    return;
  }
  std::vector<Card> cards = buildCards(all, now, extra);
  if (!nextIds.empty()) {
    std::vector<Card> f;
    for (auto &c : cards)
      if (!(c.kind == CARD_GAME && c.game.state == 'p' && CFG.has(nextIds, c.game.key()))) f.push_back(c);
    cards = f;
  }
  if (favOnly || CFG.sports == "favorites") {  // drop non-favorite games, keep everything else
    std::vector<Card> f;
    for (auto &c : cards)
      if (c.kind != CARD_GAME || c.pri >= PRI_FAV_FINAL) f.push_back(c);
    cards = f;
  }
  rot.update(withClock(makePlaylist(cards), CFG.clockEvery));
}

static String nextFavText(time_t now) {
  const Game *best = nullptr;
  for (auto &lg : CFG.leagues)
    for (auto &g : games[lg])
      if (g.state == 'p' && isFavoriteGame(g) && g.start > now && (!best || g.start < best->start)) best = &g;
  if (!best) return "";
  const Team &mine = CFG.isFavorite(best->league, best->home.abbr) ? best->home : best->away;
  return mine.abbr + " " + fmtStart(best->start, now);
}

// ------------------------------------------------------------------ celebrations
static void celebrate(const ScoreEvent &ev) {
  const Team &t = ev.home ? ev.game.home : ev.game.away;
  HoldBanners hold;
  petCheerPending = true;             // the pet cheers and the robot eyes light up for a minute
  excitedUntil = millis() + 60000;
  uint32_t c1 = teamColor(t), c2 = teamAlt(t);
  if (soundReady && !(CFG.quietAtNight && isNight(nowUtc()))) soundPlay(ev.sound, t.abbr);
  uint32_t t0 = millis();
  bool flip = false;
  while (millis() - t0 < 4000 || (soundBusy() && millis() - t0 < 12000)) {
    uint32_t bg = flip ? c1 : c2;
    uint32_t fg = luminance(bg) > 120 ? 0x000000 : 0xFFFFFF;
    drawCelebrateFrame(t.abbr, ev.label, bg, fg);
    flip = !flip;
    pump(350);
  }
}

// ------------------------------------------------------------------ show one card
static bool busyNow() {
  if (busyMode != BUSY_AUTO) return isBusy(meetings, nowUtc(), busyMode);
  String label;
  uint32_t col;
  if (presAvail.length() && presenceBusy(presAvail, presActivity, label, col)) return true;  // Teams says busy
  return isBusy(meetings, nowUtc(), BUSY_AUTO);
}

// bottom-left of the clock: next meeting countdown, else next favorite game, else the date
static String clockLeft(time_t now, uint32_t &color) {
  const Meeting *m = nextMeeting(meetings, now);
  if (m && m->start - now <= CFG.meetingSoonMinutes * 60L) {
    color = m->start - now <= 5 * 60 ? 0xFF2020 : 0xFFC000;
    return "MTG " + durationText(m->start - now);
  }
  String label;
  if (presAvail.length()) {  // Teams: "IN CALL", "MEETING", "DND", "AWAY"
    presenceBusy(presAvail, presActivity, label, color);
    if (label.length()) return label;
  }
  return "";
}

// scroll a line across the panel: returns when the dwell is over and the text went by at least once
template <typename Draw>
static int scrollFor(const String &txt, int dwellMs, int x0, Draw draw) {
  int w = textWidth(txt);
  bool fits = w <= 64 - x0 - 2;
  int x = fits ? x0 : 64;
  uint32_t t0 = millis();
  bool passed = fits;
  while (millis() - t0 < (uint32_t)dwellMs || !passed) {
    draw(x);
    if (!fits && --x < -w) {
      x = 64;
      passed = true;
    }
    int r = pump(fits ? 250 : 35);
    if (r) return r;
    if (millis() - t0 > 30000) break;  // safety
  }
  return 0;
}

// gravity in the rotated screen's own terms
static void logicalGravity(int rot, float &x, float &y) {
  const Motion &m = motion();
  switch (rot) {
    case 1: x = m.gy, y = -m.gx; break;
    case 2: x = -m.gx, y = -m.gy; break;
    case 3: x = -m.gy, y = m.gx; break;
    default: x = m.gx, y = m.gy;
  }
}

// batch 3 screens; -1 = not one of them
static int showCard3(const Card *c) {
  const bool accel = motion().present;
  auto animate = [&](int frameMs, std::function<void(int)> draw) {
    uint32_t t0 = millis();
    for (int f = 0; millis() - t0 < (uint32_t)c->dwell * 1000; f++) {
      draw(f);
      int r = pump(frameMs);
      if (r) return r;
    }
    return 0;
  };
  auto textPages = [&](const String &title, uint32_t tc, const String &right, uint32_t rc, const String &body) {
    std::vector<String> lines = wrapText(foldAscii(body), 10);
    int maxOff = std::max(0, (int)lines.size() * 8 - 21);
    drawTextCard(title, tc, right, rc, lines, 0);
    int r = pump(maxOff ? 2200 : c->dwell * 1000);
    if (r) return r;
    for (int off = 1; off <= maxOff; off++) {
      drawTextCard(title, tc, right, rc, lines, off);
      r = pump(80);
      if (r) return r;
    }
    return maxOff ? pump(2500) : 0;
  };
  switch (c->kind) {
    case CARD_AQUARIUM: return animate(40, [&](int f) { drawAquarium(f); });
    case CARD_PENDULUM: return animate(33, [&](int f) { drawDoublePendulum(f, f == 0); });
    case CARD_CRADLE:
      return animate(33, [&](int f) {
        float gx, gy;
        logicalGravity(displayRotation(), gx, gy);
        bool kick = cardTapped;
        cardTapped = false;
        drawCradle(f, gx, gy, accel, kick, f == 0);
      });
    case CARD_WATER:
      return animate(40, [&](int f) { drawWater(f, motion().gx, motion().gy, accel, false); });
    case CARD_BEER:
      return animate(33, [&](int f) {
        float gx, gy;
        logicalGravity(displayRotation(), gx, gy);
        bool tap = cardTapped;
        cardTapped = false;
        drawBeer(f, gx, gy, accel, tap, f == 0);
      });
    case CARD_PCLOCK: {
      uint32_t t0 = millis();
      while (millis() - t0 < (uint32_t)c->dwell * 1000) {
        drawPortraitClock(nowUtc());
        int r = pump(500);
        if (r) return r;
      }
      return 0;
    }
    case CARD_HOURGLASS:
      return animate(40, [&](int f) {
        float gx, gy;
        logicalGravity(displayRotation(), gx, gy);
        drawHourglass(f, gy, accel, f == 0);
      });
    case CARD_LAVA: return animate(50, [&](int f) { drawLava(f, false); });
    case CARD_FLYW: {
      uint32_t col = 0x005A9C;  // Dodger blue; other teams get a bright blue
      if (!flyW.us.equalsIgnoreCase("LAD")) col = 0x2F6BFF;
      GameResult r = flyW;
      return animate(50, [&](int f) { drawFlyW(r, col, f); });
    }
    case CARD_ISS: {
      IssFix f0 = iss;
      return animate(100, [&](int f) { drawIssMap(f0, nowUtc(), CFG.lat, CFG.lon, f); });
    }
    case CARD_PLANETS: {
      drawPlanets(nowUtc(), CFG.lat, CFG.lon, 0);
      int r = pump(c->dwell * 600);
      if (r) return r;
      drawPlanets(nowUtc(), CFG.lat, CFG.lon, 1);
      return pump(c->dwell * 400);
    }
    case CARD_SURF: {
      SurfReport s0 = surf;
      return animate(60, [&](int f) { drawSurf(s0, CFG.surfName, f); });
    }
    case CARD_TRIVIA: {
      if (triviaIdx >= trivia.size()) return 0;
      TriviaQ q = trivia[triviaIdx++];
      String cat = q.category.substring(0, 5);
      cat.toUpperCase();
      int r = textPages("TRIVIA", 0xB070FF, cat, 0x707070, q.question);
      if (r) return r;
      r = pump(3000);  // think...
      if (r) return r;
      return textPages("ANSWER", 0x20FF40, "", 0, q.answer);
    }
    case CARD_FACT:
      if (factQuote.author.length())
        return textPages("QUOTE", 0x40C0FF, "", 0, factQuote.text + "  - " + factQuote.author);
      return textPages("FUN FACT", 0xFFC000, "", 0, factQuote.text);
    case CARD_BILLBOARD: {
      if (charts.empty()) return 0;
      const ChartWeek &w = charts[chartIdx++ % charts.size()];
      String title = "#1 IN " + w.date.substring(0, 4);
      String body = w.top[0].song + " - " + w.top[0].artist;
      if (w.top.size() > 1 && w.top[1].song.length()) body += "  #2 " + w.top[1].song;
      return textPages(title, 0xFFD040, String(w.yearsAgo) + "Y", 0x707070, body);
    }
    case CARD_NATDAY: {
      String body;
      for (size_t i = 0; i < natDays.size() && i < 4; i++) body += (i ? "  " : "") + natDays[i] + ".";
      return textPages("TODAY IS", 0xFF70B0, "", 0, body);
    }
    case CARD_HN: {
      for (size_t i = 0; i < hnTitles.size() && i < 3; i++) {
        int r = textPages("HACKER NEWS", 0xFF6600, "", 0, hnTitles[i]);
        if (r) return r;
      }
      return 0;
    }
    default: return -1;
  }
}

// the favorite's next game, with the scoreboard's line, TV and colours when it has them
static NextGame nextGameFor(const String &key) {
  NextGame n;
  for (auto &x : nextGames)
    if (x.league + ":" + x.us == key) n = x;
  if (!n.ok) return n;
  for (auto &g : games[n.league]) {
    const Team *us = g.away.abbr == n.us ? &g.away : g.home.abbr == n.us ? &g.home : nullptr;
    if (us) n.usColor = teamColor(*us);
    const Team *them = g.away.abbr == n.them ? &g.away : g.home.abbr == n.them ? &g.home : nullptr;
    if (them) n.themColor = teamColor(*them);
    if (g.id == n.id) {
      n.line = g.line;
      n.overUnder = g.overUnder;
      if (g.tv.length()) n.tv = g.tv;
    }
  }
  n.tv.toUpperCase();
  return n;
}

// batch 4 screens; -1 = not one of them
static int showCard4(const Card *c) {
  auto animate = [&](int frameMs, std::function<void(int)> draw) {
    uint32_t t0 = millis();
    for (int f = 0; millis() - t0 < (uint32_t)c->dwell * 1000; f++) {
      draw(f);
      int r = pump(frameMs);
      if (r) return r;
    }
    return 0;
  };
  switch (c->kind) {
    case CARD_BINARY: return animate(250, [&](int) { drawBinaryClock(nowUtc()); });
    case CARD_SUNARC: return animate(1000, [&](int) { drawSunArcClock(nowUtc(), CFG.lat, CFG.lon); });
    case CARD_CITY: return animate(60, [&](int f) { drawCityClock(nowUtc(), f); });
    case CARD_WORLD: return animate(1000, [&](int) { drawWorldClock(nowUtc()); });
    case CARD_SAND:
      return animate(40, [&](int f) {
        float gx = 0, gy = 1;
        if (motion().present) logicalGravity(displayRotation(), gx, gy);
        drawSandClock(nowUtc(), gx, gy, f == 0);
      });
    case CARD_LIFE:
      return animate(40, [&](int f) {
        bool poke = cardTapped;
        cardTapped = false;
        drawLife(f, f == 0, poke);
      });
    case CARD_VISUALIZER: {
      micUse(true, CFG.clapControl);
      uint8_t b[16];
      int r = animate(33, [&](int f) {
        int n = micBands(b, 16);
        drawVisualizer(b, n, micPresent(), f);
      });
      micUse(false, CFG.clapControl);
      return r;
    }
    case CARD_FLIGHTS: {
      std::vector<Flight> f0 = flights;
      int n = std::max<int>(1, std::min<int>(3, f0.size()));
      return animate(50, [&](int f) { drawFlights(f0, (f / 60) % n, CFG.flightRadiusKm, f); });
    }
    case CARD_WINPROB: {
      auto it = winProbs.find(c->text);
      if (it == winProbs.end()) return 0;
      WinProb w = it->second;
      for (auto &g : games[w.game.league])  // fresher score and clock than the last summary download
        if (g.id == w.game.id) w.game = g;
      return animate(250, [&](int f) { drawWinProb(w, nowUtc(), (f / 10) % 3 == 2); });
    }
    case CARD_GOLF: {
      GolfBoard g0 = golf;
      int pages = golfPages(g0);
      for (int p = 0; p < pages; p++) {
        drawGolf(g0, p);
        int r = pump(3000);
        if (r) return r;
      }
      return 0;
    }
    case CARD_NEXTGAME: {
      NextGame n = nextGameFor(c->text);
      if (!n.ok) return 0;
      return animate(500, [&](int f) { drawNextGame(n, nowUtc(), (f / 5) % 2 == 1); });
    }
    case CARD_HIGHLIGHT: {
      auto it = highlights.find(c->text);
      if (it == highlights.end()) return 0;
      std::vector<Highlight> hs = it->second;
      Game g;
      for (auto &lg : CFG.leagues)
        for (auto &x : games[lg])
          if (x.key() == c->text) g = x;
      for (auto &h : hs) {
        const Team &us = g.home.abbr == h.team ? g.home : g.away, &them = g.home.abbr == h.team ? g.away : g.home;
        String res = g.id.length() ? String(us.score > them.score ? "W " : us.score < them.score ? "L " : "T ") +
                                         String(us.score) + "-" + String(them.score)
                                   : String("FINAL");
        drawHighlight(h, res);
        int r = pump(4000);
        if (r) return r;
      }
      return 0;
    }
    case CARD_FPTS: {
      FptsBoard b = fpts;
      int r = 0;
      uint32_t t0 = millis();
      for (int f = 0; millis() - t0 < 4500; f++) {  // the matchup, then each side's players
        drawFantasyTotals(b, f);
        if ((r = pump(100))) return r;
      }
      for (int side = 0; side < 2; side++)
        for (int p = 0; p < fantasyPages(b, side); p++) {
          t0 = millis();
          for (int f = 0; millis() - t0 < 3500; f++) {
            drawFantasyPage(b, side, p, f);
            r = pump(100);
            if (r) return r;
          }
        }
      return 0;
    }
    default: return -1;
  }
}

// ------------------------------------------------------------------ batch 5 helpers
static void petLoad() {
  Preferences p;
  p.begin("pet", true);
  uint8_t buf[96];
  size_t n = p.getBytes("s", buf, sizeof buf);
  p.end();
  if (!n || !petUnpack(pet, buf, n)) pet = PetState();
}
static void petSave() {
  uint8_t buf[96];
  size_t n = petPack(pet, buf, sizeof buf);
  if (!n) return;
  Preferences p;
  p.begin("pet", false);
  p.putBytes("s", buf, n);
  p.end();
  petSavedAt = millis();
}
// the pet lives on whether or not its card is showing: called from the main loop
static void petHousekeeping() {
  time_t now = nowUtc();
  if (now < 1700000000) return;  // no clock time yet
  if (!pet.born) {
    pet = PetState();
    pet.born = pet.lastUpdate = pet.lastSeen = (uint32_t)now;
    petSave();
  }
  petTick(pet, (uint32_t)now, isNight(now));
  if (petCheerPending) {
    petCheer(pet, (uint32_t)now);
    petCheerPending = false;
  }
  if (millis() - petSavedAt > 10 * 60 * 1000UL) petSave();
}

// "BUF VS MIA SUN 1:00 CBS": the favorites' next game, for the split-flap board's second row
static String nextGameLine(time_t now, uint32_t &color, time_t *start = nullptr, String *abbr = nullptr) {
  const NextGame *best = nullptr;
  for (auto &n : nextGames)
    if (n.ok && n.start + 4 * 3600 > now && (!best || n.start < best->start)) best = &n;
  if (!best) return "";
  NextGame n = nextGameFor(best->league + ":" + best->us);
  if (!n.ok) n = *best;
  color = n.usColor;
  if (start) *start = n.start;
  if (abbr) *abbr = n.us;
  String s = n.us + (n.home ? " VS " : " @ ") + n.them + " " + fmtStart(n.start, now);
  if (n.tv.length()) s += " " + n.tv;
  s.toUpperCase();
  return s;
}

// a news alert drops down over whatever is on screen, scrolls its headline once, and goes back up
static void showNewsBanner() {
  if (newsQueue.empty()) return;
  NewsItem n = newsQueue.front();
  newsQueue.erase(newsQueue.begin());
  for (auto &x : newsItems)
    if (x.id == n.id) x.read = true;
  HoldBanners hold;
  if (CFG.newsSound && soundReady && !(CFG.quietAtNight && isNight(nowUtc()))) soundPlay("chime");
  int w = textWidth(n.headline);
  int x = 64;
  uint32_t t0 = millis();
  for (int f = 0; millis() - t0 < 20000; f++) {
    drawNewsBanner(n, f, f < 8 ? 64 : x);
    if (f >= 8 && --x < -w && millis() - t0 > 4000) break;
    int r = pump(30);
    if (r && r != 7) return;  // turned / button / phone: drop the banner
  }
}

// sand timer, set mode: each tap (or a wheel turn) adds 15 seconds; 3 s without one starts it; double tap cancels
static void runSandSetup() {
  HoldBanners hold;
  displaySetRotation(motionLandscapeRotation());
  int setSec = 0;
  uint32_t lastTap = millis();
  for (int f = 0; millis() - lastTap < (setSec ? 3000u : 10000u); f++) {
    drawSandTimer(0, 0, setSec, f);
    int r = pump(33);
    if (r == 1) {  // single tap / wheel
      setSec = std::min(setSec + 15, 99 * 60);
      lastTap = millis();
      if (soundReady) soundPlay("xp");
    } else if (r) {
      return;
    }
  }
  if (setSec) {
    sandTotal = setSec;
    sandEnd = nowUtc() + setSec;
  }
}

// a running sand timer owns the screen (like the focus timer): tap = +15 s, double tap / wheel press = stop
static void runSand() {
  HoldBanners hold;
  displaySetRotation(motionLandscapeRotation());
  uint32_t t0 = millis();
  static int frame = 0;
  while (millis() - t0 < 1000) {
    long remain = (long)(sandEnd - nowUtc());
    if (remain <= 0) break;
    drawSandTimer((int)sandTotal, (int)remain, 0, frame++);
    int r = pump(33);
    if (r == 1) {
      sandTotal += 15;
      sandEnd += 15;
    } else if (r == 2 || r == 6) {
      sandTotal = 0;
      drawMessage("TIMER", "stopped", 0x707070);
      pump(1000);
      return;
    } else if (r) {
      break;
    }
  }
  if (sandTotal && (long)(sandEnd - nowUtc()) <= 0) {  // time's up
    if (soundReady) soundPlay("focus");
    uint32_t t1 = millis();
    for (int f = 0; millis() - t1 < 6000; f++) {
      drawSandTimer((int)sandTotal, 0, 0, f);
      if (pump(33)) break;
    }
    sandTotal = 0;
  }
}

// batch 5 screens; -1 = not one of them
static int showCard5(const Card *c) {
  auto animate = [&](int frameMs, std::function<void(int)> draw) {
    uint32_t t0 = millis();
    for (int f = 0; millis() - t0 < (uint32_t)c->dwell * 1000; f++) {
      draw(f);
      int r = pump(frameMs);
      if (r) return r;
    }
    return 0;
  };
  auto scrollItem = [&](int minMs, int textW, std::function<void(int)> draw) {  // draw(x): x scrolls once
    int x = 64;
    uint32_t t0 = millis();
    while (millis() - t0 < 30000) {
      draw(x);
      if (--x < -textW && millis() - t0 > (uint32_t)minMs) break;
      int r = pump(35);
      if (r) return r;
    }
    return 0;
  };
  switch (c->kind) {
    case CARD_FLAP: {
      uint32_t col = 0xFFC000;
      String sub = nextGameLine(nowUtc(), col);
      return animate(33, [&](int f) {
        if (drawFlapClock(nowUtc(), sub, col, f) && f > 0 && CFG.flapSound && soundReady &&
            !(CFG.quietAtNight && isNight(nowUtc())))
          soundPlay("flap");
      });
    }
    case CARD_DIALS: {
      uint32_t col = 0xFFFFFF;
      time_t start = 0;
      String abbr;
      nextGameLine(nowUtc(), col, &start, &abbr);
      return animate(250, [&](int) { drawDialsClock(nowUtc(), wx, start, abbr, col); });
    }
    case CARD_TERMINAL: return animate(50, [&](int f) { drawTerminalClock(nowUtc(), f); });
    case CARD_SLOTS: return animate(33, [&](int f) { drawSlotClock(nowUtc(), f); });
    case CARD_WEEKS:
      return animate(100, [&](int f) {
        drawLifeWeeks(nowUtc(), CFG.birthYear, CFG.birthMonth, CFG.birthDay, CFG.lifeYears, f);
      });
    case CARD_EYES:
      return animate(33, [&](int f) {
        float gx = 0, gy = 0.3f;
        if (motion().present) logicalGravity(displayRotation(), gx, gy);
        int mood = (int32_t)(excitedUntil - millis()) > 0 ? 1 : isNight(nowUtc()) ? 2 : 0;
        drawEyes(f, gx, gy, mood);
      });
    case CARD_EIGHTBALL:
      return animate(33, [&](int f) {
        bool go = cardShaken || cardTapped;
        cardShaken = cardTapped = false;
        drawEightBall(f, go);
      });
    case CARD_PARTY: {
      static int tool = 0;
      return animate(33, [&](int f) {
        if (cardTapped) {
          cardTapped = false;
          tool = (tool + 1) % 3;
        }
        bool go = cardShaken;
        cardShaken = false;
        drawParty(tool, f, go);
      });
    }
    case CARD_MAZE: return animate(33, [&](int f) { drawMaze(f); });
    case CARD_PET: {
      petHousekeeping();
      pet.lastSeen = (uint32_t)nowUtc();
      uint32_t lastPlay = 0;
      bool cheer = (int32_t)(excitedUntil - millis()) > 50000;  // a score in the last ten seconds
      int r = animate(33, [&](int f) {
        uint32_t now = (uint32_t)nowUtc();
        bool fed = cardTapped;
        cardTapped = false;
        if (fed) petFeed(pet, now);
        float gx = 0, gy = 1;
        if (motion().present) logicalGravity(displayRotation(), gx, gy);
        drawPet(pet, now, isNight(nowUtc()), f, fed, cheer && f < 60, gx, gy, motion().present);
        if (petBallSpeed() > 0.6f && millis() - lastPlay > 500) {
          lastPlay = millis();
          petPlay(pet, now);
        }
      });
      petSave();
      return r;
    }
    case CARD_LAUNCH: {
      if (launches.empty()) return 0;
      Launch l = launches[0];
      return animate(100, [&](int f) { drawLaunch(l, nowUtc(), f); });
    }
    case CARD_PLAYOFF: {
      int colon = c->text.indexOf(':');
      String lg = c->text.substring(0, colon), team = c->text.substring(colon + 1);
      auto it = standings.find(lg);
      if (it == standings.end()) return 0;
      StandGroup grp;
      for (auto &g : it->second)
        for (auto &r : g.rows)
          if (r.abbr == team) grp = g;
      if (grp.rows.empty()) return 0;
      uint32_t col = 0xFFFFFF;
      for (auto &g : games[lg]) {
        if (g.home.abbr == team) col = teamColor(g.home);
        if (g.away.abbr == team) col = teamColor(g.away);
      }
      return animate(100, [&](int f) { drawPlayoff(grp, team, col, f); });
    }
    case CARD_WAITS: {
      ParkWaits p = parkWaits;
      int pages = std::max(1, parkPages(p));
      for (int pg = 0; pg < pages; pg++) {
        uint32_t t0 = millis();
        for (int f = 0; millis() - t0 < 4000; f++) {
          drawParkWaits(p, pg, f);
          int r = pump(100);
          if (r) return r;
        }
      }
      return 0;
    }
    case CARD_STOCK: {
      StockQuote q;
      for (auto &x : stockQuotes)
        if (x.symbol == c->text) q = x;
      if (!q.ok) return 0;
      return animate(100, [&](int f) { drawStock(q, f); });
    }
    case CARD_NEWS: {
      std::vector<NewsItem> items = newsItems;
      int n = std::min<int>(5, items.size());
      for (int i = 0; i < n; i++) {
        int r = scrollItem(3500, textWidth(items[i].headline), [&](int x) { drawNewsList(items, i, x); });
        if (r) return r;
      }
      for (auto &x : newsItems) x.read = true;
      return 0;
    }
    case CARD_LASTPLAY: {
      int colon = c->text.indexOf(':');
      String lg = c->text.substring(0, colon);
      Game g;
      for (auto &x : games[lg])
        if (x.key() == c->text) g = x;
      if (!g.id.length()) return 0;
      return scrollItem(c->dwell * 1000, textWidth(g.lastPlay), [&](int x) { drawLastPlay(g, x); });
    }
    case CARD_DRIVE: return animate(250, [&](int f) { drawDriveTime(drive, nowUtc(), f); });
    case CARD_LOTTO: return animate(100, [&](int f) { drawLotto(lotto, nowUtc(), f); });
    case CARD_PICKEM: {
      std::vector<PickemPick> picks = CFG.pickem;
      std::vector<Game> all;
      for (auto &kv : games) all.insert(all.end(), kv.second.begin(), kv.second.end());
      int pages = std::max(1, pickemPages(picks));
      for (int pg = 0; pg < pages; pg++) {
        drawPickem(picks, all, pickemW, pickemL, pg);
        int r = pump(4000);
        if (r) return r;
      }
      return 0;
    }
    default: return -1;
  }
}

static int showCardInner(const Card *c);

static int showCard(const Card *c) {
  int k = c ? c->kind : CARD_CLOCK;
  if (isPortraitCard(k)) displaySetRotation(motion().present && motion().orient == 3 ? 3 : 1);
  else if (isAnyOrientCard(k)) displaySetRotation(0);
  else displaySetRotation(motionLandscapeRotation());
  cardWantsTaps = (k == CARD_CRADLE || k == CARD_BEER || k == CARD_LIFE || k == CARD_EIGHTBALL || k == CARD_PARTY ||
                   k == CARD_PET);
  cardWantsShake = (k == CARD_EIGHTBALL || k == CARD_PARTY);
  cardTapped = cardShaken = false;
  int r = c ? showCard5(c) : -1;
  if (r < 0 && c) r = showCard4(c);
  if (r < 0 && c) r = showCard3(c);
  if (r < 0) r = showCardInner(c);
  cardWantsTaps = cardWantsShake = false;
  return r;
}

static int showCardInner(const Card *c) {
  if (msLoginPending()) {  // Outlook sign-in: show the code until it's entered
    drawMessage(msUserCode(), "see phone", 0xFFC000);
    int r = pump(2000);
    return r == 1 ? 4 : r;
  }
  if (msLoginOk) {
    msLoginOk = false;
    drawMessage("OUTLOOK", "signed in", 0x20FF40);
    pump(1500);
  }
  if (!c || c->kind == CARD_CLOCK) {
    uint32_t t0 = millis();
    int dwell = c ? c->dwell : 5;
    do {
      time_t now = nowUtc();
      uint32_t col = 0;
      String left = clockLeft(now, col);
      if (!left.length() && CFG.sports != "off") {
        left = nextFavText(now);
        col = 0x707070;
        if (left.length() > 7) left = "";
      }
      struct tm lt;
      localtime_r(&now, &lt);
      int hol = holidayOn(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
      int frameMs = 1000;
      if (CFG.clockStyle == "big") drawBigClock(now);
      else if (CFG.clockStyle == "nixie") {
        drawNixieClock(now, 0);
        frameMs = 120;  // the tubes flicker
      } else if (CFG.clockStyle == "words") drawWordClock(now);
      else if (CFG.clockStyle == "tetris") {
        static bool tetrisFirst = true;
        frameMs = drawTetrisClock(now, tetrisFirst) ? 35 : 250;
        tetrisFirst = false;
      } else drawClockFace(now, wx, left, col, busyNow(), hol >= 0 ? holidayInfo(hol).color : 0x707070);
      int r = pump(frameMs);
      if (r) return r;
    } while (millis() - t0 < (uint32_t)dwell * 1000);
    return 0;
  }
  if (c->kind == CARD_MEETING) {
    bool busy = busyNow();
    return scrollFor(c->text, c->dwell * 1000, 2, [&](int x) { drawMeeting(*c, x, busy); });
  }
  if (c->kind == CARD_TODO) {
    if (c->items.empty()) return 0;
    for (int shown = 0; shown < 3 && shown < (int)c->items.size(); shown++) {
      int i = todoIdx++ % c->items.size();
      int r = scrollFor(c->items[i], 2500, 0, [&](int x) { drawTodo(*c, i, x); });
      if (r) return r;
    }
    return 0;
  }
  if (c->kind == CARD_COUNTDOWN) {
    drawCountdown(*c);
    return pump(c->dwell * 1000);
  }
  if (c->kind == CARD_MUSIC) {
    NowPlaying m = currentMusic();
    return scrollFor(m.title, c->dwell * 1000, 9, [&](int x) {
      uint32_t p = m.progressMs + (m.playing ? millis() - m.fetchedAt : 0);
      drawMusic(m, p, x);
    });
  }
  if (c->kind == CARD_GAME) {
    uint32_t t0 = millis();
    bool phase = false;
    auto draw = [&](bool ph) {
      if (c->game.league == "nfl") drawFootball(c->game, nowUtc(), ph);
      else if (c->game.league == "mlb") drawBaseball(c->game, nowUtc());
      else drawGame(c->game, nowUtc(), ph);
    };
    draw(false);
    while (millis() - t0 < (uint32_t)c->dwell * 1000) {
      int r = pump(250);
      if (r) return r;
      bool p = ((millis() - t0) / 2500) % 2 == 1;
      if (p != phase) {
        phase = p;
        draw(phase);
      }
    }
    // live NFL: the last play scrolls across the bottom once
    if (c->game.league == "nfl" && c->game.state == 'i' && c->game.lastPlay.length()) {
      int w = textWidth(c->game.lastPlay);
      for (int x = 64; x > -w; x -= 2) {
        drawFootball(c->game, nowUtc(), false, x);
        int r = pump(30);
        if (r) return r;
      }
    }
    return 0;
  }
  if (c->kind == CARD_STANDINGS) {
    drawStandings(*c, true);
    int r = pump(1500);
    if (r) return r;
    drawStandings(*c, false);
    return pump(c->dwell * 1000);
  }
  if (c->kind == CARD_HISTORY || c->kind == CARD_WORD || c->kind == CARD_JOKE) {
    String title, right, body;
    uint32_t tc = YELLOW_C, rc = 0x707070;
    if (c->kind == CARD_HISTORY) {
      const HistoryEvent &h = history[historyIdx++ % history.size()];
      title = "TODAY";
      tc = 0x40A0FF;
      right = String(h.year);
      rc = YELLOW_C;
      body = h.text;
    } else if (c->kind == CARD_WORD) {
      title = wotd.word;
      String pos = wotd.pos;
      pos.toLowerCase();
      right = pos.startsWith("noun") ? "n." : pos.startsWith("verb") ? "v." : pos.startsWith("adj") ? "adj." : pos.startsWith("adv") ? "adv." : "";
      body = wotd.def;
    } else {
      title = "DAD JOKE";
      tc = 0xFF8C00;
      body = joke;
    }
    std::vector<String> lines = wrapText(foldAscii(body), 10);
    int maxOff = std::max(0, (int)lines.size() * 8 - 21);  // until the last line is on screen
    drawTextCard(title, tc, right, rc, lines, 0);
    int r = pump(maxOff ? 1800 : c->dwell * 1000);
    if (r) return r;
    for (int off = 1; off <= maxOff; off++) {
      drawTextCard(title, tc, right, rc, lines, off);
      r = pump(75);
      if (r) return r;
    }
    return maxOff ? pump(2500) : 0;
  }
  if (c->kind == CARD_IMAGES) return playNextImage(c->dwell * 1000, pump);
  if (c->kind == CARD_FIRE) return playYuleLog(c->dwell * 1000, pump);
  if (c->kind == CARD_NIXIE || c->kind == CARD_BIGCLOCK || c->kind == CARD_WORDCLOCK) {
    uint32_t t0 = millis();
    for (int f = 0; millis() - t0 < (uint32_t)c->dwell * 1000; f++) {
      if (c->kind == CARD_NIXIE) drawNixieClock(nowUtc(), f);
      else if (c->kind == CARD_BIGCLOCK) drawBigClock(nowUtc());
      else drawWordClock(nowUtc());
      int r = pump(c->kind == CARD_NIXIE ? 120 : 500);
      if (r) return r;
    }
    return 0;
  }
  if (c->kind == CARD_TETRIS) {
    uint32_t t0 = millis();
    bool first = true;
    while (millis() - t0 < (uint32_t)c->dwell * 1000) {
      bool falling = drawTetrisClock(nowUtc(), first);
      first = false;
      int r = pump(falling ? 35 : 250);
      if (r) return r;
    }
    return 0;
  }
  if (c->kind == CARD_RAIN || c->kind == CARD_FIRE || c->kind == CARD_BOUNCE || c->kind == CARD_SNAKE ||
      c->kind == CARD_BREAKOUT) {
    const int frameMs = c->kind == CARD_SNAKE ? 70 : c->kind == CARD_FIRE ? 40 : 30;
    uint32_t t0 = millis();
    for (int f = 0; millis() - t0 < (uint32_t)c->dwell * 1000; f++) {
      switch (c->kind) {
        case CARD_RAIN: drawDigitalRain(f); break;
        case CARD_FIRE: drawFireplace(f); break;
        case CARD_BOUNCE: drawBouncer(f); break;
        case CARD_SNAKE: drawSnake(f); break;
        default: drawBreakout(f); break;
      }
      int r = pump(frameMs);
      if (r) return r;
    }
    return 0;
  }
  if (c->kind == CARD_HOLIDAY) {
    uint32_t t0 = millis();
    int frame = 0;
    while (millis() - t0 < (uint32_t)c->dwell * 1000) {
      drawHoliday(*c, frame++);
      int r = pump(60);
      if (r) return r;
    }
    return 0;
  }
  if (c->kind == CARD_UFC) {
    return scrollFor(c->text, c->dwell * 1000, 1, [&](int x) { drawUfc(*c, nowUtc(), x); });
  }
  if (c->kind == CARD_SUNMOON) {
    drawSunMoon(nowUtc());
    return pump(c->dwell * 1000);
  }
  if (c->kind == CARD_PROGRESS) {
    drawProgress(nowUtc());
    return pump(c->dwell * 1000);
  }
  if (c->kind == CARD_WEATHER) {
    drawWeatherDetail(wx);
    return pump(c->dwell * 1000);
  }
  if (c->kind == CARD_YAHOO) {
    drawYahoo(*c);
    return pump(c->dwell * 1000);
  }
  // fantasy: scroll the stat line across the bottom
  int width = c->text.length() * 6;
  int x = width > 64 ? 64 : 0;
  uint32_t t0 = millis();
  while (millis() - t0 < (uint32_t)max(c->dwell * 1000, (width + 64) * 35)) {
    drawScrollFrame(c->title, c->live ? 0xFFC000 : 0x707070, c->text, x);
    if (width > 64 && --x < -width) x = 64;
    int r = pump(35);
    if (r) return r;
  }
  return 0;
}

// ------------------------------------------------------------------ focus timer + meeting chime
static void startFocus(int minutes, bool brk) {
  focusOn = true;
  onBreak = brk;
  focusTotal = minutes * 60L;
  focusEnd = nowUtc() + focusTotal;
}

static void stopFocus() { focusOn = false; }

static bool quietNow() { return CFG.quietAtNight && isNight(nowUtc()); }

// The tap menu (double tap on any ordinary screen): tap = next entry, double tap (or the wheel press / BOOT hold) =
// choose, 6 s of nothing = back to the screens. Entries are focus, games, favorites and clock.
static int tapMenu() {
  HoldBanners hold;
  static const char *const items[] = {"FOCUS", "GAMES", "FAVS ONLY", "CLOCK", "TIMER"};
  constexpr int n = sizeof items / sizeof items[0];
  displaySetRotation(motionLandscapeRotation());
  int sel = 0;
  uint32_t last = millis();
  while (millis() - last < 6000) {
    int first = std::max(0, sel - 3);  // the menu shows 4 rows: scroll to keep the choice in view
    drawTapMenu(sel - first, items + first, std::min(4, n - first));
    int r = pump(120);
    if (r == 1) {
      sel = (sel + 1) % n;
      last = millis();
    } else if (r == 2 || r == 6) {
      return sel;
    } else if (r == 4 || r == 5) {
      return -1;  // something else took over (phone page, turned, shaken)
    }
  }
  return -1;
}

static void chooseClockFace() {
  HoldBanners hold;
  const char *const names[] = {"CLASSIC", "BIG DIGITS", "NIXIE", "WORDS", "TETRIS", "P: DIGITS", "P: ANALOG"};
  const char *const styles[] = {"classic", "big", "nixie", "words", "tetris", "stacked", "analog"};
  constexpr int count = sizeof styles / sizeof styles[0];
  int sel = 0;
  for (int i = 0; i < 5; i++)
    if (CFG.clockStyle == styles[i]) sel = i;
  uint32_t last = millis();
  while (millis() - last < 10000) {
    drawMessage(names[sel], "2 TAP=SET", 0xFFD16A);
    int action = pump(120);
    if (action == 1) {
      sel = (sel + 1) % count;
      last = millis();
    } else if (action == 2 || action == 6) {
      const bool portrait = sel >= 5;
      if (portrait) CFG.portraitClockStyle = styles[sel];
      else CFG.clockStyle = styles[sel];

      JsonDocument config;
      String json = readConfigText();
      bool saved = !deserializeJson(config, json) && config.is<JsonObject>();
      if (saved) {
        config["clock"][portrait ? "portrait_style" : "style"] = styles[sel];
        json = "";
        serializeJson(config, json);
        saved = writeConfigText(json);
      }
      drawMessage(names[sel], saved ? "SAVED" : "THIS BOOT", 0xFFD16A);
      pump(800);
      return;
    } else if (action != 0) {
      return;
    }
  }
}

static void runFocus() {
  time_t now = nowUtc();
  long remain = (long)(focusEnd - now);
  if (remain > 0 && motionPortrait()) {
    displaySetRotation(motion().present && motion().orient == 3 ? 3 : 1);
    drawPortraitFocus(remain, focusTotal, onBreak);
    displaySetRotation(motionLandscapeRotation());
  } else {
    drawFocus(remain, focusTotal, onBreak, now);
  }
  if (remain > 0) {
    int r = pump(1000);
    if (r == 2 || r == 6) {
      stopFocus();
      drawMessage(onBreak ? "BREAK" : "FOCUS", "stopped", 0x707070);
      pump(1200);
    } else if (r == 3) {
      favOnly = !favOnly;
    }
    return;
  }
  if (soundReady && !quietNow()) soundPlay("focus");
  if (!onBreak && CFG.breakMinutes > 0) {
    drawMessage("NICE WORK", "break time", 0x20FF40);
    pump(3000);
    startFocus(CFG.breakMinutes, true);
  } else {
    drawMessage(onBreak ? "BREAK OVER" : "DONE", "", 0xFF6020);
    pump(3000);
    stopFocus();
  }
}

static void checkChime() {
  if (CFG.chimeMinutes <= 0) return;
  HoldBanners hold;
  time_t now = nowUtc();
  const Meeting *m = nextMeeting(meetings, now);
  if (!m || m->start == lastChimeFor || m->start - now > CFG.chimeMinutes * 60L) return;
  lastChimeFor = m->start;
  if (soundReady && !quietNow()) soundPlay("chime");
  String when = "IN " + durationText(m->start - now);
  uint32_t t0 = millis();
  bool flip = false;
  while (millis() - t0 < 5000) {
    drawCelebrateFrame("MEETING", when, flip ? 0x40A0FF : 0x000000, flip ? 0x000000 : 0x40A0FF);
    flip = !flip;
    if (pump(400)) break;
  }
}

static void updateStatus() {
  time_t now = nowUtc();
  String s = "Busy light: ";
  s += busyMode == BUSY_AUTO ? "auto" : (busyMode == BUSY_FORCE ? "forced busy" : "forced free");
  s += busyNow() ? " (busy now)" : " (free now)";
  if (focusOn) s += " | " + String(onBreak ? "Break " : "Focus ") + durationText((long)(focusEnd - now)) + " left";
  const Meeting *m = nextMeeting(meetings, now);
  if (m) s += " | Next meeting: " + m->subject + " at " + clockText(m->start);
  webStatusLine = s;
}

// ------------------------------------------------------------------ setup / loop
// ------------------------------------------------------------------ background downloads (core 0)
static void netTask(void *) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED && time(nullptr) > 1700000000) {
      if (msLoginRequested) {
        msLoginRequested = false;
        Lock lock;
        msStartLogin();
      }
      if (msLoginPending()) {
        Lock lock;
        if (msPollLogin() == 1) {
          msLoginOk = true;
          nextCalendar = 0;
          todoSetMicrosoft({});
        }
      }
      if (webCheckUpdate) {  // phone page button: check right away, before the (slower) data refresh
        webCheckUpdate = false;
        checkForUpdate();  // may restart the board
      }
      refreshData();
      autoUpdateTick(time(nullptr));  // may restart the board
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  netInitTls();
  dataMtx = xSemaphoreCreateRecursiveMutex();
  xSemaphoreTakeRecursive(dataMtx, portMAX_DELAY);  // the display loop owns the data except during pump()
  Serial.begin(115200);
  pinMode(BTN_BOOT, INPUT_PULLUP);
  bool expOk = expanderBegin();  // also switches the audio chips' power on
  if (expOk) wheelIdle = expanderRead();
  bool sdOk = sdBegin();
  Serial.printf("[sd] %s\n", sdOk ? "card found" : "NO CARD (using built-in settings, no sound)");
  loadSettings();
  motionBegin();  // ADXL345 on the I2C port (optional)
  petLoad();
  {
    Preferences pr;  // pick'em season record
    pr.begin("pickem", true);
    pickemW = pr.getInt("w", 0);
    pickemL = pr.getInt("l", 0);
    pr.end();
  }
  orientKey = motionPortrait() ? 'P' : 'L';
  Serial.printf("[display] driver=%s brightness=%d swap_gb=%d\n", CFG.panelDriver.c_str(), CFG.brightness,
                CFG.swapGreenBlue);
  while (!displayBegin()) {  // without a display nothing else is useful; say so instead of crashing
    Serial.println("[display] init FAILED (check panel.driver / memory). Retrying in 5 s");
    delay(5000);
  }
  Serial.println("[display] started; showing red, green, blue test");
  displayTestPattern();
  drawMessage("PIXEL", "PRESSBOX", 0xFFC000);
  if (!sdOk) {
    drawMessage("NO SD CARD", "defaults", 0xFF2020);
    delay(1500);
  }
  soundReady = sdOk && soundBegin();
  if (soundReady) micUse(false, CFG.clapControl);
  if (sdOk) {
    todoLoad();
    imagesScan();
  }
  {
    Preferences p;
    p.begin("screen", true);
    pinKey = p.getString("pin", "");
    p.end();
  }

  WiFiManager wm;
  wm.setConfigPortalTimeout(300);
  wm.setAPCallback([](WiFiManager *) { drawMessage("JOIN WIFI:", "PixelPress", 0x20FF40); });
  drawMessage("WIFI...", "");
  // Every network the clock has joined is remembered: use whichever one is in range. If none is (or it's the
  // first boot), WiFiManager tries the last one, then opens the "PixelPressbox-Setup" network to add a new one.
  wifiLoad();
  if (!wifiConnectSaved() && !wm.autoConnect("PixelPressbox-Setup")) ESP.restart();
  wifiRemember(WiFi.SSID(), WiFi.psk());

  configTzTime(CFG.tz.c_str(), "pool.ntp.org", "time.nist.gov");
  drawMessage("SYNC", "clock...");
  for (int i = 0; i < 40 && nowUtc() < 1700000000; i++) delay(250);
  webBegin();
  Serial.printf("Ready: http://pixelpressbox.local  (%s)  firmware %s\n", WiFi.localIP().toString().c_str(), FW_VERSION);
  drawMessage("LOADING", "");
  if (soundReady) soundPlay("startup");
  otaSetMainTask();
  xTaskCreatePinnedToCore(netTask, "net", 16384, nullptr, 1, nullptr, 0);
}

// ---- games ----------------------------------------------------------------------------------------------
// The game loop keeps the essentials of pump() running (phone page, sound, net handover, taps) but not the
// orientation check: hard tilting is the whole point here. Double tap or the phone's Stop quits.
static int bestCache[G_COUNT];
static bool bestLoaded = false;
static int gameBest(int id) {
  if (bestLoaded) return bestCache[id];
  Preferences p;
  p.begin("games", true);
  for (int i = 0; i < G_COUNT; i++) bestCache[i] = p.getInt(("b" + String(i)).c_str(), 0);
  p.end();
  bestLoaded = true;
  return bestCache[id];
}
static void gameSaveBest(int id, int v) {
  Preferences p;
  p.begin("games", false);
  p.putInt(("b" + String(id)).c_str(), v);
  p.end();
  bestCache[id] = v;
}

// One slice of housekeeping. Returns: 0 nothing, 1 tap, 2 double tap / quit, 10+N phone asked for game N,
// 99 phone asked for the menu.
static int gameTick() {
  xSemaphoreGiveRecursive(dataMtx);
  vTaskDelay(1);
  xSemaphoreTakeRecursive(dataMtx, portMAX_DELAY);
  soundLoop();
  webLoop();
  motionPoll();
  if (webWantsRestart) {
    delay(300);
    ESP.restart();
  }
  if (webGameCmd != -2) {
    int c = webGameCmd;
    webGameCmd = -2;
    if (c == -1) return 2;
    if (c == 99) return 99;
    return 10 + c;
  }
  if (webNext) {
    webNext = false;
    return 2;
  }
  if (motion().present) {
    motionTakeShake();  // shaking is fair play inside a game
    int t = motionTakeTap();
    if (t) return t;
  }
  if (int w = pollWheel()) return w == 1 ? 1 : 2;  // wheel: turn = tap, press = quit
  return 0;
}

static GameInput readTilt(float basePitch, int rotation) {
  GameInput in;
  const Motion &m = motion();
  float s = rotation == 2 ? -1 : 1;  // upside down: left/right and back/forward swap
  if (rotation & 1) {  // portrait game: "right" is the turned screen's right edge
    float gx, gy;
    logicalGravity(rotation, gx, gy);
    in.roll = constrain(gx / 0.5f, -1.0f, 1.0f);
  } else {
    in.roll = constrain(s * m.gx / 0.5f, -1.0f, 1.0f);
  }
  float pv = s * -(m.gz - basePitch) / 0.5f;
  if (CFG.pitchInvert) pv = -pv;
  in.pitch = constrain(pv, -1.0f, 1.0f);
  return in;
}

static void runGames(int start) {
  // standing on its end: the portrait game (BLOCKFALL) starts straight away; the menu is landscape-only
  if (motionPortrait() && (start < 0 || start >= G_COUNT)) start = G_BLOCKFALL;
  int rotation = motionLandscapeRotation();
  displaySetRotation(rotation);
  bool tilt = motion().present;
  int sel = start >= 0 && start < G_COUNT ? start : 0;
  bool inMenu = start < 0 || start >= G_COUNT;
  auto skipPortrait = [&](int dir) {  // the landscape menu skips portrait-only games
    for (int k = 0; k < G_COUNT && gameIsPortrait(sel); k++) sel = (sel + dir + G_COUNT) % G_COUNT;
  };
  int frame = 0;
  while (true) {
    if (inMenu) {  // tilt left/right to choose, tap to play; idle 20 s = back to the cards
      rotation = motionLandscapeRotation();
      displaySetRotation(rotation);
      skipPortrait(1);
      uint32_t idle = millis(), menuOpened = idle, lastMove = 0;
      while (true) {
        int t = gameTick();
        if (t == 2) return;
        if (t >= 10 && t < 10 + G_COUNT) {
          sel = t - 10;
          break;
        }
        if (t == 1 && millis() - menuOpened >= 400) break;
        if (tilt) {
          float r = readTilt(0, rotation).roll;
          if (fabsf(r) > 0.45f && millis() - lastMove > 450) {
            sel = (sel + (r > 0 ? 1 : G_COUNT - 1)) % G_COUNT;
            skipPortrait(r > 0 ? 1 : -1);
            lastMove = idle = millis();
          }
        } else if (millis() - idle > 2500) {  // no accelerometer: the menu flips through, then demos one
          sel = (sel + 1) % G_COUNT;
          skipPortrait(1);
          if (millis() - idle > 6000) break;
        }
        if (millis() - idle > 20000) return;
        drawGameMenu(sel, gameBest(sel), frame++);
        delay(33);
      }
    }
    inMenu = false;
    // portrait games turn the screen (whichever way the clock leans); the others use the landscape rotation
    rotation = gameIsPortrait(sel) ? (motion().present && motion().orient == 3 ? 3 : 1) : motionLandscapeRotation();
    displaySetRotation(rotation);
    // ready
    uint32_t t0 = millis();
    while (millis() - t0 < 1500) {
      int t = gameTick();
      if (t == 2) return;
      drawGameReady(sel, frame++);
      delay(33);
    }
    float basePitch = tilt ? motion().gz : 0;  // "lean back" is measured from how you were holding it
    gameReset(sel, esp_random());
    uint32_t started = millis(), next = millis();
    int quit = 0;
    while (true) {
      int t = gameTick();
      if (t == 2 || t == 99 || t >= 10) {
        quit = t;
        break;
      }
      GameInput in = tilt ? readTilt(basePitch, rotation) : gameAutopilot(sel);
      in.tap = tilt ? t == 1 : in.tap;
      if (!gameStep(sel, in)) break;
      if (!tilt && millis() - started > 60000) break;  // demo mode: a minute each
      gameDraw(sel, frame++);
      next += 33;
      int32_t wait = (int32_t)(next - millis());
      if (wait > 0) delay(wait);
      else next = millis();
    }
    if (quit == 2) return;
    if (quit == 99) {
      inMenu = true;
      continue;
    }
    if (quit >= 10) {
      sel = quit - 10;
      continue;
    }
    int sc = gameScore(sel), best = gameBest(sel);
    bool nb = tilt && sc > best;
    if (nb) {
      gameSaveBest(sel, sc);
      if (soundReady) soundPlay("win");
    }
    t0 = millis();
    bool again = false;
    while (millis() - t0 < 5000) {  // tap: play again; double tap: done; otherwise back to the menu
      int t = gameTick();
      if (t == 2) return;
      if (t == 1 && millis() - t0 > 700) {
        again = true;
        break;
      }
      if (t >= 10 && t < 10 + G_COUNT) {
        sel = t - 10;
        again = true;
        break;
      }
      drawGameOver(sel, sc, nb ? sc : best, nb, frame++);
      delay(33);
    }
    if (!again) {
      if (gameIsPortrait(sel) && motionPortrait()) return;  // still on its end: back to the tall screens
      if (!tilt) sel = (sel + 1) % G_COUNT;  // demo: move on to the next game
      inMenu = tilt;
    }
  }
}

void loop() {
  time_t now = nowUtc();
  displaySetBrightness(flatSleeping ? 0 : isNight(now) ? CFG.nightBrightness : CFG.brightness);
  soundSetVolume(CFG.volume);

  static uint32_t wifiLostAt = 0;
  if (WiFi.status() != WL_CONNECTED) {
    drawMessage("NO WIFI", "retrying", 0xFF2020);
    if (!wifiLostAt) wifiLostAt = millis();
    if (millis() - wifiLostAt > 20000) {  // still gone: look for any of the other remembered networks
      wifiLostAt = millis();
      if (!wifiConnectSaved(10000)) WiFi.reconnect();
    } else {
      WiFi.reconnect();
    }
    pump(3000);
    return;
  }
  wifiLostAt = 0;
  displaySetRotation(motionLandscapeRotation());  // messages, focus, celebrations: landscape (or upside down)
  rebuild();  // cheap; keeps "IN 12M" style titles fresh. The rotator keeps its place.
  updateStatus();
  while (!pendingCelebrations.empty()) {
    celebrate(pendingCelebrations.front());
    pendingCelebrations.erase(pendingCelebrations.begin());
  }
  checkChime();

  if (focusOn) {  // the focus timer owns the screen until it ends or you press the wheel
    runFocus();
    return;
  }
  if (sandTotal) {  // so does a running sand timer
    runSand();
    return;
  }
  static uint32_t lastPetTick = 0;
  if (millis() - lastPetTick > 60000) {  // the pet's needs move on even when its screen isn't up
    lastPetTick = millis();
    petHousekeeping();
  }

  updateCardsJson();
  if (pinKey.length() && !rot.select(pinKey)) rot.select("clock");  // pinned card gone for now (game over?): clock
  int r = showCard(rot.current());
  if (r == 7) {  // a news alert: show it, then carry on with the same card
    showNewsBanner();
    return;
  }
  if (r == 5) {
    int g = webGameCmd;
    webGameCmd = -2;
    runGames(g == 99 ? -1 : g);
    motionTakeTap();
    orientKey = 0;  // re-check the orientation on the way out
    return;
  }
  if (r == 6) {  // double tap: the tap menu (on its end it just starts the focus timer, as before)
    int c = motionPortrait() ? 0 : tapMenu();
    if (c == 0) {
      startFocus(CFG.focusMinutes, false);
    } else if (c == 1) {
      runGames(-1);
      motionTakeTap();
      orientKey = 0;
    } else if (c == 2) {
      favOnly = !favOnly;
      drawMessage(favOnly ? "FAVS ONLY" : "ALL GAMES", "", 0xFFC000);
      pump(1000);
    } else if (c == 3) {
      chooseClockFace();
    } else if (c == 4) {
      runSandSetup();
    }
    return;
  }
  if (r == 2) {
    startFocus(CFG.focusMinutes, false);
  } else if (r == 3) {
    favOnly = !favOnly;
    drawMessage(favOnly ? "FAVS ONLY" : "ALL GAMES", "", 0xFFC000);
    pump(1000);
  } else if (r != 4 && rot.current()) {
    if (pinKey.length()) {
      if (r == 1) {  // wheel/Next while pinned: move the pin to the next card
        rot.advance();
        if (rot.current()) pinKey = pinKeyOf(*rot.current());
        savePin();
      }
    } else {
      rot.advance();
    }
  }
}
