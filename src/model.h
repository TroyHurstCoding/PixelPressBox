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
  bool historyGentle = true;               // skip "on this day" items about deaths and disasters
  bool teamsPresence = true;               // Teams status drives the busy light (needs Outlook sign-in)
  int ufcDays = 10;                        // show it when the event is this many days away (or live)


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
