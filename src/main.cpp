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
#include "spotify.h"
#include "todo.h"
#include "weather.h"
#include "espn.h"
#include "playlist.h"
#include <functional>
#include "render2.h"
#include "feeds2.h"
#include "motion.h"
#include "render.h"
#include "scorewatch.h"
#include "settings.h"
#include "sound.h"
#include "web.h"
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
// 0 = timed out
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
      if (CFG.flatSleep) {  // lying flat: screen off until it's picked up
        if (m.flat) {
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
      if (CFG.accelTaps) {
        int tap = motionTakeTap();
        if (tap == 1) {
          if (cardWantsTaps) cardTapped = true;
          else return 1;  // tap: next card
        } else if (tap == 2) {
          return cardWantsTaps ? 1 : 2;  // double tap: next (on interactive screens) / focus timer
        }
      }
    }
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
    std::vector<Card> cards;
    for (auto &lg : CFG.leagues)
      for (auto &g : games[lg]) {
        if (g.state == 'p') continue;
        std::vector<FantasyPlayer> want;
        for (auto &p : CFG.fantasy)
          if (p.league == g.league && (p.team == g.away.abbr || p.team == g.home.abbr)) want.push_back(p);
        if (want.empty()) continue;
        std::map<String, StatLine> lines;
        if (!fetchBoxLines(g, want, lines)) continue;
        for (auto &kv : lines) {
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
    {
      Lock lock;
      fantasyCards = cards;
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
    {
      Lock lock;  // msgraph keeps its token in shared state; calendar calls are quick
      okM = msFetchCalendar(now, m);
      okF = msFetchFlagged(f);
      if (okM) meetings = m;
      if (okF) flagged = f;
    }
    nextCalendar = millis() + CFG.pollCalendar * 1000UL;
    changed = true;
  }

  // standings: every 6 hours per league, in season only
  for (auto &lg : CFG.leagues) {
    if (!CFG.cardStandings || !inSeason(lg, now) || (int32_t)(millis() - nextStandings[lg]) < 0) continue;
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
    std::vector<std::pair<CardKind, const char *>> tall;
    if (CFG.cardBeer) tall.push_back({CARD_BEER, "beer"});
    if (CFG.cardHourglass) tall.push_back({CARD_HOURGLASS, "hourglass"});
    if (CFG.cardLava) tall.push_back({CARD_LAVA, "lava"});
    if (CFG.cardWater) tall.push_back({CARD_WATER, "water"});
    for (auto &t : tall) {
      if (CFG.cardPClock) add(CARD_PCLOCK, "pclock", CFG.clockDwell);
      add(t.first, t.second, 20);
    }
    if (p.empty()) add(CARD_PCLOCK, "pclock", CFG.clockDwell);
    rot.update(p);
    return;
  }
  std::vector<Card> cards = buildCards(all, now, extra);
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

static int showCardInner(const Card *c);

static int showCard(const Card *c) {
  int k = c ? c->kind : CARD_CLOCK;
  if (isPortraitCard(k)) displaySetRotation(motion().present && motion().orient == 3 ? 3 : 1);
  else if (isAnyOrientCard(k)) displaySetRotation(0);
  else displaySetRotation(motionLandscapeRotation());
  cardWantsTaps = (k == CARD_CRADLE || k == CARD_BEER);
  cardTapped = false;
  int r = c ? showCard3(c) : -1;
  if (r < 0) r = showCardInner(c);
  cardWantsTaps = false;
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

static void runFocus() {
  time_t now = nowUtc();
  long remain = (long)(focusEnd - now);
  drawFocus(remain, focusTotal, onBreak, now);
  if (remain > 0) {
    int r = pump(1000);
    if (r == 2) {
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
  if (!wm.autoConnect("PixelPressbox-Setup")) ESP.restart();

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

void loop() {
  time_t now = nowUtc();
  displaySetBrightness(flatSleeping ? 0 : isNight(now) ? CFG.nightBrightness : CFG.brightness);
  soundSetVolume(CFG.volume);

  if (WiFi.status() != WL_CONNECTED) {
    drawMessage("NO WIFI", "retrying", 0xFF2020);
    WiFi.reconnect();
    pump(3000);
    return;
  }
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

  updateCardsJson();
  if (pinKey.length() && !rot.select(pinKey)) rot.select("clock");  // pinned card gone for now (game over?): clock
  int r = showCard(rot.current());
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
