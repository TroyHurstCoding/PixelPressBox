// model.h - shared data types and settings.
#pragma once
#include <Arduino.h>
#include <vector>

struct Team {
  String abbr = "?";
  String id;
  String record;
  uint32_t color = 0xFFFFFF;
  uint32_t alt = 0x000000;
  int score = 0;
};

struct Game {
  String id;
  String league;        // "nfl" | "nba" | "mlb"
  time_t start = 0;     // UTC epoch seconds
  char state = 'p';     // 'p' pre, 'i' in progress, 'f' final
  String detail;        // ESPN shortDetail, e.g. "4:12 - 3rd", "Bot 7th", "Final"
  String clock;
  int period = 0;
  Team away, home;
  // situation (live only)
  bool hasSit = false;
  String poss;          // NFL: team id with the ball
  String down;          // NFL: "3rd & 7"
  String spot;          // NFL: "LV 35" (ball on)
  String lastPlay;      // NFL: "J.Daniels pass deep left ... INTERCEPTED by J.Holland" (cleaned up)
  int awayTO = -1, homeTO = -1;  // NFL timeouts left
  bool redzone = false;
  int outs = 0;         // MLB
  int balls = 0, strikes = 0;
  bool on1 = false, on2 = false, on3 = false;
  // pre-game extras (scoreboard)
  String tv;            // "CBS"
  String line;          // "KC -3.5"
  String overUnder;     // "47.5"

  String key() const { return league + ":" + id; }
};

struct FantasyPlayer {
  String name, league, team;
};

// one team's line in a standings table
struct StandRow {
  String abbr;
  int wins = 0, losses = 0, ties = 0, seed = 0;
  float pct = 0;
  String gb;            // games behind ("-" for the leader)
};
// a division or conference table
struct StandGroup {
  String league, name;  // "nfl", "AFC West"
  std::vector<StandRow> rows;  // sorted best first
};

struct HistoryEvent {
  int year = 0;
  String text;
};

struct WordOfDay {
  bool ok = false;
  String word, pos, def;  // "gauntlet", "noun", "a series of challenges..."
};

// a UFC card from ESPN's schedule (times UTC)
struct FightEvent {
  String label;            // "UFC 334: Gane vs. Hokit"
  time_t start = 0, end = 0;
};

// ---- batch 3 data cards
struct TriviaQ {
  String category, question, answer;
};
struct FactQuote {
  String text, author;  // author "" = a fun fact
};
struct ChartEntry {
  String song, artist;
};
struct ChartWeek {
  bool ok = false;
  int yearsAgo = 0;
  String date;                     // "2006-11-11"
  std::vector<ChartEntry> top;     // #1..#3
};
struct SurfReport {
  bool ok = false;
  float waveFt = 0, periodS = 0;
  int dirDeg = 0, waterF = 0;
  float maxFtToday = 0;
};
struct IssFix {
  bool ok = false;
  float lat = 0, lon = 0;
  uint32_t at = 0;                 // millis() of the fix (we extrapolate between fetches)
  float dLonPerMin = 3.8f;         // drift in longitude per minute (the ISS laps the earth in ~92 min)
};
struct GameResult {
  bool ok = false;
  bool won = false;
  String us, them;                 // abbreviations
  int usScore = 0, themScore = 0;
  time_t when = 0;                 // game start (UTC)
};

// ---- batch 4 data
// win probability line for one game (ESPN summary), home team's chance 0-100 per play
struct WinProb {
  bool ok = false;
  String gameKey;
  Game game;                       // snapshot for teams/colors/status
  std::vector<uint8_t> home;
};
// a team's top performers after a game (ESPN summary "leaders")
struct Highlight {
  String gameKey, team, who, what; // "BUF", "J. ALLEN", "304 YDS 1 TD"
  uint32_t color = 0xFFFFFF;
};
struct GolfRow {
  String pos, name, score;         // "T2", "S. SCHEFFLER", "-12"
};
struct GolfBoard {
  bool ok = false;
  String event, detail;            // "THE RSM CLASSIC", "Round 3 - In Progress"
  char state = 'p';
  time_t start = 0;
  std::vector<GolfRow> rows;       // top 10, leaderboard order
};
// a favorite team's next game (ESPN team schedule, plus the scoreboard's line when it has one)
struct NextGame {
  bool ok = false;
  String league, id, us, them, tv, line, overUnder;
  bool home = false;
  time_t start = 0;
  uint32_t usColor = 0xFFFFFF, themColor = 0xA0A0A0;
};
struct Flight {
  String callsign;
  int altFt = 0, speedKt = 0, heading = 0, climbFpm = 0;
  float distKm = 0, bearing = 0;   // from home
};

// fantasy points (NFL): players chosen on the phone page, scored with the league's own rules
struct FptsPlayer {
  String id, name, pos, team;      // id = ESPN athlete id; a team defence has pos "DST" and id ""
  int side = 0;                    // 0 = my team, 1 = opponent
};
struct FptsLine {                  // one player's week so far
  String name, pos, team, stats;   // stats: "21/30 254yd 2TD"
  float pts = 0;
  char state = 'n';                // 'p' pre, 'i' live, 'f' final, 'n' no game (bye)
  bool hasBall = false;            // live and on the field (offense with the ball, or a defence without it)
  int side = 0;
};
struct FptsBoard {
  bool ok = false;
  String meLabel, oppLabel;
  float me = 0, opp = 0;
  bool anyLive = false, anyStarted = false;
  int week = 0;
  std::vector<FptsLine> lines;
};

struct WorldCity {
  String name;                     // "LON"
  int utcMin = 0;                  // standard time offset from UTC in minutes
  String dst;                      // "us" | "eu" | "au" | "" (none)
};

// ---- batch 5 data
struct Launch {                    // Launch Library 2 (thespacedevs.com)
  String id, name, rocket, mission, pad, provider, status;  // status: "Go", "TBD", "TBC", "Hold", "Success"...
  time_t net = 0;                  // UTC launch time ("no earlier than")
  bool vandenberg = false;         // visible from Southern California
};
struct RideWait {                  // queue-times.com
  String name;
  int wait = 0;                    // minutes
  bool open = false;
};
struct ParkWaits {
  bool ok = false;
  String park;                     // "DISNEYLAND"
  time_t fetched = 0;
  std::vector<RideWait> rides;     // open rides, longest wait first
};
struct StockQuote {                // Robinhood stock-token prices (api.robinhood.com/rhj/prices)
  String symbol;
  bool ok = false;
  float price = 0, bid = 0, ask = 0, dayHigh = 0, dayLow = 0;
  float prevClose = 0;             // last price the clock saw on the previous trading day (0 = unknown)
  float open = 0;                  // first price the clock saw today
  bool halted = false;
  time_t at = 0;
  std::vector<float> spark;        // today's samples, oldest first (at most 64)
};
struct NewsItem {                  // ESPN news, filtered for the owner
  String id, league, headline, why;  // why: "BUF" / "J.ALLEN" / "TRADE": what matched
  time_t published = 0;
  uint32_t color = 0xFFFFFF;       // team colour when known
  bool read = false;
};
struct DriveTime {                 // TomTom routing with live traffic
  bool ok = false;
  String label;                    // "WORK"
  int minutes = 0, typical = 0, delay = 0;   // now, without traffic, traffic delay (minutes)
  float km = 0;
  time_t at = 0;
};
struct LottoDraw {                 // Powerball
  bool ok = false;
  time_t drawDate = 0;             // last draw (local date at 00:00 UTC of that day)
  int white[5] = {0, 0, 0, 0, 0};
  int power = 0, multiplier = 0;
  String jackpot;                  // "$412 MILLION" when known, else ""
  time_t nextDraw = 0;             // next draw, UTC
};
struct PickemPick {                // one pick: the game and the team picked to win
  String league, gameId, pick;     // pick = team abbr
  char result = ' ';               // ' ' pending, 'W', 'L', 'T' (push)
};

struct Countdown {
  String name;
  int year = 0, month = 0, day = 0;  // local date; year 0 = every year (birthdays, anniversaries)
  uint32_t color = 0xFFC000;
};

// one calendar event (times are UTC epoch seconds)
struct Meeting {
  String subject;
  time_t start = 0, end = 0;
  String showAs;        // free | tentative | busy | oof | workingElsewhere | unknown
  bool allDay = false;
};

struct Weather {
  bool ok = false;
  int temp = 0, hi = 0, lo = 0;
  int code = 0;         // WMO weather code (Open-Meteo)
  bool day = true;
  int feels = 0, uv = 0, rainPct = 0;
  int aqi = -1;         // US AQI, -1 = unknown
};

struct NowPlaying {
  bool playing = false;
  String title, artist;
  uint32_t progressMs = 0, durationMs = 0;
  uint32_t fetchedAt = 0;  // millis() when fetched, to animate the progress bar
};

struct Settings {
  // ---- desk clock
  int clockDwell = 20;                     // seconds the clock stays up between other cards
  int clockEvery = 2;                      // show the clock again after this many other cards
  int cardDwell = 7;
  String sports = "all";                   // all | favorites | off
  float lat = 0, lon = 0;                  // weather location (0,0 = weather off)
  bool fahrenheit = true;
  int pollWeather = 900;
  // Outlook / Microsoft 365 (device-code sign-in from the phone page)
  String msClientId, msTenant = "organizations";
  bool msFlaggedMail = true;
  bool msTodo = false;
  int pollCalendar = 180;
  int chimeMinutes = 2;                    // meeting chime this many minutes before start (0 = off)
  int meetingSoonMinutes = 60;             // clock shows "MTG 12M" when a meeting is this close
  bool busyLight = true;                   // red frame around the clock while you're in a busy meeting
  // Spotify (refresh token from tools/spotify_auth.py)
  String spClientId, spSecret, spRefresh;
  // focus timer
  int focusMinutes = 25, breakMinutes = 5;
  std::vector<Countdown> countdowns;
  // holidays: "all" | "federal" | "off"; countdown shows when the next one is this many days away
  String holidays = "all";
  int holidayCountdownDays = 21;
  std::vector<String> holidaySkip;         // names to hide, e.g. "Groundhog Day"
  // optional cards
  bool cardSunMoon = true, cardProgress = true, cardWeather = true;
  bool ufc = true;                         // next UFC event card
  bool cardStandings = true, cardHistory = true, cardWord = true, cardJoke = true;
  bool cardTetris = true, cardRain = true;
  String rainWord;                         // word hidden in the digital rain ("" = none)
  bool cardFire = true, cardBounce = true, cardSnake = true, cardBreakout = true, cardImages = true;
  String bounceText = "PIXEL";             // the bouncing logo's text
  String portraitClockStyle = "stacked";
  String clockStyle = "classic";           // main clock: classic | big | nixie | words | tetris
  bool cardNixie = true, cardBigClock = true, cardWordClock = true;  // extra clock styles in the rotation
  bool historyGentle = true;               // skip "on this day" items about deaths and disasters
  bool teamsPresence = true;               // Teams status drives the busy light (needs Outlook sign-in)
  int ufcDays = 10;                        // show it when the event is this many days away (or live)
  // batch 3: physics / ambient
  bool cardAquarium = true, cardPendulum = true, cardCradle = true, cardWater = true;
  // portrait-only screens (shown when the clock stands on its end, or orientation = "portrait")
  bool cardBeer = true, cardPClock = true, cardHourglass = true, cardLava = true;
  // batch 3: data cards
  bool cardTrivia = true, cardFacts = true, cardBillboard = true, cardFlyW = true;
  bool cardIss = true, cardPlanets = true, cardNationalDay = true, cardSurf = true, cardHackerNews = true;
  std::vector<int> billboardYears{20, 30, 40};  // "this week N years ago"
  // batch 4 screens
  bool cardBinary = true, cardSunArc = true, cardCity = true, cardWorld = true, cardSand = true;
  bool cardWinProb = true, cardGolf = true, cardNextGame = true, cardHighlights = true;
  bool cardVisualizer = true, cardLife = true, cardFlights = true, cardFpts = true;
  bool clapControl = false;                // two claps = next screen (needs the microphones)
  uint8_t micGain = 0x1A;                  // ES7210 PGA gain register (0x16 low .. 0x1C very high)
  std::vector<WorldCity> worldClocks{{"LA", -480, "us"}, {"NYC", -300, "us"}, {"LON", 0, "eu"}, {"TOKYO", 540, ""}};
  int flightRadiusKm = 40;
  String golfTour = "pga";
  // fantasy points
  String fptsMe = "MY TEAM", fptsOpp = "OPPONENT";
  int fptsWeek = 0;                        // 0 = this week
  std::vector<FptsPlayer> fptsPlayers;
  std::vector<std::pair<String, float>> fptsRules;     // e.g. ("passTD", 5); see fantasy.cpp for the defaults
  std::vector<std::pair<int, float>> fptsDstTiers;     // (max points allowed, fantasy points), first match wins
  // batch 5 screens
  bool cardFlap = true, cardDials = true, cardTerminal = true, cardSlots = true, cardWeeks = true;
  bool cardEyes = true, cardEightBall = true, cardParty = true, cardMaze = true, cardPet = true;
  bool cardLaunch = true, cardPlayoff = true, cardWaits = true, cardStocks = true, cardNews = true;
  bool cardLastPlay = true, cardDrive = true, cardLotto = true, cardPickem = true;
  bool flapSound = true;                   // split-flap clock clicks when the minute changes (if sound is on)
  int birthYear = 0, birthMonth = 1, birthDay = 1;  // life in weeks (0 = card off)
  int lifeYears = 80;
  String parkId = "16", parkName = "DISNEYLAND";    // queue-times.com park id (16 Disneyland, 17 California Adventure)
  std::vector<String> stocks{"SPY", "QQQ", "AAPL"};  // Robinhood stock-token symbols
  // news alerts (ESPN): banner over the current screen
  bool newsAlerts = true, newsSound = true;
  bool newsFavorites = true, newsFantasy = true;     // teams in favorites / players in fantasy_points
  std::vector<String> newsKeywords{"trade", "traded", "injury", "injured", "out for", "torn", "signs", "released"};
  std::vector<String> newsLeagues{"nfl"};
  int newsQuietStart = 22, newsQuietEnd = 7;         // no banners (they still collect) in these hours
  // drive time (TomTom key from developer.tomtom.com, free tier)
  String tomtomKey, driveLabel = "WORK", driveAddress;
  float driveLat = 0, driveLon = 0;                  // found from driveAddress
  int driveFrom = 6, driveTo = 10;                   // hours (local) it's a priority card on weekdays
  // pick'em: picks made on the phone page
  std::vector<PickemPick> pickem;
  String surfName = "VENICE";
  float surfLat = 33.985f, surfLon = -118.475f;  // Venice Breakwater
  // accelerometer (ADXL345): orientation auto | landscape | portrait; mirror flips which way portrait turns;
  // tap = next card, double tap = focus timer, lying flat = screen off
  String orientation = "auto";
  bool accelMirror = false, accelTaps = true, flatSleep = true;
  String accelMount = "panel";             // where the accelerometer sits: "panel" (flat on the back of the LED
                                           // panel, pins along the lower edge) | "fin" (the earlier spot, chip-down
                                           // under a fin on the back cover)
  String tapForce = "light";               // how hard a tap must be: "verylight" | "light" | "firm" (the original)
  bool pitchInvert = false;                // games: flip "lean back" if it comes out backwards


  std::vector<String> leagues{"nfl", "nba", "mlb"};
  // favorites / show_all / celebrate keyed by league
  std::vector<std::pair<String, String>> favorites;  // (league, team abbr)
  std::vector<String> showAll{"nfl"};
  std::vector<String> celebrate{"nfl", "mlb"};
  std::vector<String> celebrateAll;
  std::vector<FantasyPlayer> fantasy;

  int takeoverMixEvery = 3;
  int finalsHours = 12;
  int upcomingHours = 36;
  int dwellFavLive = 10, dwellDefault = 5;
  int pollLive = 20, pollSoon = 60, pollIdle = 300, pollOff = 900, pollFantasy = 60, pollYahoo = 120;

  String tz = "PST8PDT,M3.2.0,M11.1.0";  // POSIX TZ string (Pacific, with DST)
  bool twelveHour = true;
  uint8_t brightness = 160;                // 0-255
  int nightStart = 23, nightEnd = 7;
  uint8_t nightBrightness = 25;

  // panel
  String panelDriver = "SHIFTREG";         // SHIFTREG | FM6126A | ICN2038S | FM6124 | MBI5124
  bool swapGreenBlue = false;              // Adafruit #5036 panels need true
  bool clkPhase = false;
  bool panelFlip = false;                  // the LED panel is installed the other way up: turn every picture half a turn

  // sound
  bool soundOn = true;
  uint8_t volume = 15;                     // 0-21
  bool quietAtNight = true;

  // yahoo (filled from config.json "yahoo" block)
  String yClientId, ySecret, yRefresh, yTeamKey, yRedirect = "oob";
  String updatePassword = "pixelpressbox";
  // nightly updates from GitHub (firmware/manifest.json in the project repo)
  bool autoUpdate = true;
  int autoUpdateHour = 3;
  String autoUpdateManifest = "https://raw.githubusercontent.com/TroyHurstCoding/PixelPressBox/main/firmware/manifest.json";  // phone-page firmware updates (user "admin"); change it in config.json

  bool isFavorite(const String &league, const String &abbr) const {
    for (auto &f : favorites) if (f.first == league && f.second == abbr) return true;
    return false;
  }
  static bool has(const std::vector<String> &v, const String &s) {
    for (auto &x : v) if (x == s) return true;
    return false;
  }
};

extern Settings CFG;
