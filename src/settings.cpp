#include "settings.h"
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include "board.h"

Settings CFG;

bool sdBegin() {
  SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
  return SD.begin(SD_CS, SPI, 20000000);
}

static void strList(JsonVariantConst v, std::vector<String> &out) {
  if (!v.is<JsonArrayConst>()) return;
  out.clear();
  for (JsonVariantConst x : v.as<JsonArrayConst>()) out.push_back(x.as<String>());
}

// accepts {"nfl": true, "nba": false} or ["nfl"]
static void leagueFlags(JsonVariantConst v, std::vector<String> &out) {
  if (v.is<JsonArrayConst>()) return strList(v, out);
  if (!v.is<JsonObjectConst>()) return;
  out.clear();
  for (JsonPairConst kv : v.as<JsonObjectConst>())
    if (kv.value().as<bool>()) out.push_back(kv.key().c_str());
}

bool loadSettings(const char *path) {
  File f = SD.open(path);
  if (!f) {
    Serial.println("[cfg] no config.json on SD card, using defaults");
    CFG.favorites = {{"mlb", "LAD"}, {"nfl", "LV"}, {"nba", "LAL"}};
    return false;
  }
  JsonDocument d;
  DeserializationError err = deserializeJson(d, f);
  f.close();
  if (err) {
    Serial.printf("[cfg] config.json error: %s\n", err.c_str());
    return false;
  }
  Settings &c = CFG;
  strList(d["leagues"], c.leagues);
  c.favorites.clear();
  for (JsonPairConst kv : d["favorites"].as<JsonObjectConst>())
    for (JsonVariantConst t : kv.value().as<JsonArrayConst>()) c.favorites.push_back({kv.key().c_str(), t.as<String>()});
  leagueFlags(d["show_all"], c.showAll);
  leagueFlags(d["celebrate"], c.celebrate);
  leagueFlags(d["celebrate_all"], c.celebrateAll);
  c.fantasy.clear();
  for (JsonObjectConst p : d["fantasy_players"].as<JsonArrayConst>())
    c.fantasy.push_back({p["name"].as<String>(), p["league"].as<String>(), p["team"].as<String>()});

  c.takeoverMixEvery = d["takeover_mix_every"] | c.takeoverMixEvery;
  c.finalsHours = d["show_finals_hours"] | c.finalsHours;
  c.upcomingHours = d["show_upcoming_hours"] | c.upcomingHours;
  c.dwellFavLive = d["dwell_seconds"]["favorite_live"] | c.dwellFavLive;
  c.dwellDefault = d["dwell_seconds"]["default"] | c.dwellDefault;
  JsonObjectConst p = d["poll_seconds"];
  c.pollLive = p["live"] | c.pollLive;
  c.pollSoon = p["soon"] | c.pollSoon;
  c.pollIdle = p["idle"] | c.pollIdle;
  c.pollOff = p["offseason"] | c.pollOff;
  c.pollFantasy = p["fantasy"] | c.pollFantasy;
  c.pollYahoo = p["yahoo"] | c.pollYahoo;

  c.tz = d["timezone"] | c.tz.c_str();
  c.twelveHour = d["twelve_hour"] | c.twelveHour;
  c.brightness = d["brightness"] | c.brightness;
  c.nightStart = d["night"]["start"] | c.nightStart;
  c.nightEnd = d["night"]["end"] | c.nightEnd;
  c.nightBrightness = d["night"]["brightness"] | c.nightBrightness;

  c.panelDriver = d["panel"]["driver"] | c.panelDriver.c_str();
  c.swapGreenBlue = d["panel"]["swap_green_blue"] | c.swapGreenBlue;
  c.clkPhase = d["panel"]["clk_phase"] | c.clkPhase;

  c.soundOn = d["sound"]["enabled"] | c.soundOn;
  c.volume = d["sound"]["volume"] | c.volume;
  c.quietAtNight = d["sound"]["quiet_at_night"] | c.quietAtNight;

  // desk clock
  c.clockDwell = d["clock"]["dwell"] | c.clockDwell;
  c.clockEvery = d["clock"]["every"] | c.clockEvery;
  c.cardDwell = d["card_dwell"] | c.cardDwell;
  c.sports = d["sports"] | c.sports.c_str();
  JsonObjectConst w = d["weather"];
  c.lat = w["lat"] | c.lat;
  c.lon = w["lon"] | c.lon;
  c.fahrenheit = String(w["units"] | "F") != "C";
  c.pollWeather = w["poll"] | c.pollWeather;
  JsonObjectConst o = d["outlook"];
  c.msClientId = o["client_id"] | "";
  c.msTenant = o["tenant"] | c.msTenant.c_str();
  c.msFlaggedMail = o["flagged_email"] | c.msFlaggedMail;
  c.chimeMinutes = o["chime_minutes"] | c.chimeMinutes;
  c.meetingSoonMinutes = o["soon_minutes"] | c.meetingSoonMinutes;
  c.busyLight = o["busy_light"] | c.busyLight;
  c.pollCalendar = o["poll"] | c.pollCalendar;
  c.spClientId = d["spotify"]["client_id"] | "";
  c.spSecret = d["spotify"]["client_secret"] | "";
  c.spRefresh = d["spotify"]["refresh_token"] | "";
  c.focusMinutes = d["focus"]["minutes"] | c.focusMinutes;
  c.breakMinutes = d["focus"]["break"] | c.breakMinutes;
  c.countdowns.clear();
  for (JsonObjectConst cd : d["countdowns"].as<JsonArrayConst>()) {
    Countdown x;
    x.name = cd["name"] | "";
    x.name.toUpperCase();
    const char *date = cd["date"] | "";
    if (!x.name.length()) continue;
    if (sscanf(date, "%d-%d-%d", &x.year, &x.month, &x.day) != 3) {
      x.year = 0;  // "MM-DD" = every year
      if (sscanf(date, "%d-%d", &x.month, &x.day) != 2) continue;
    }
    String col = cd["color"] | "#FFC000";
    if (col.startsWith("#")) col = col.substring(1);
    x.color = strtoul(col.c_str(), nullptr, 16);
    c.countdowns.push_back(x);
  }

  JsonObjectConst hol = d["holidays"];
  c.holidays = hol["show"] | c.holidays.c_str();
  c.holidayCountdownDays = hol["countdown_days"] | c.holidayCountdownDays;
  c.holidaySkip.clear();
  for (JsonVariantConst s : hol["skip"].as<JsonArrayConst>()) c.holidaySkip.push_back(s.as<String>());
  JsonObjectConst cards = d["cards"];
  c.cardSunMoon = cards["sun_moon"] | c.cardSunMoon;
  c.cardProgress = cards["date_progress"] | c.cardProgress;
  c.cardWeather = cards["weather_details"] | c.cardWeather;
  c.cardStandings = cards["standings"] | c.cardStandings;
  c.cardHistory = cards["on_this_day"] | c.cardHistory;
  c.cardWord = cards["word_of_the_day"] | c.cardWord;
  c.cardJoke = cards["dad_jokes"] | c.cardJoke;
  c.cardTetris = cards["tetris_clock"] | c.cardTetris;
  c.cardRain = cards["digital_rain"] | c.cardRain;
  c.rainWord = cards["rain_word"] | "";
  c.rainWord.toUpperCase();
  c.cardFire = cards["fireplace"] | c.cardFire;
  c.cardBounce = cards["bouncing_logo"] | c.cardBounce;
  c.bounceText = cards["bounce_text"] | c.bounceText.c_str();
  c.cardSnake = cards["snake"] | c.cardSnake;
  c.cardBreakout = cards["breakout"] | c.cardBreakout;
  c.cardImages = cards["images"] | c.cardImages;
  c.cardNixie = cards["nixie_clock"] | c.cardNixie;
  c.cardBigClock = cards["big_clock"] | c.cardBigClock;
  c.cardWordClock = cards["word_clock"] | c.cardWordClock;
  c.clockStyle = d["clock"]["style"] | c.clockStyle.c_str();
  c.historyGentle = cards["on_this_day_gentle"] | c.historyGentle;
  c.teamsPresence = d["outlook"]["teams_presence"] | c.teamsPresence;
  c.updatePassword = d["update_password"] | c.updatePassword.c_str();
  c.autoUpdate = d["auto_update"]["enabled"] | c.autoUpdate;
  c.autoUpdateHour = d["auto_update"]["hour"] | c.autoUpdateHour;
  c.autoUpdateManifest = d["auto_update"]["manifest"] | c.autoUpdateManifest.c_str();
  c.cardAquarium = cards["aquarium"] | c.cardAquarium;
  c.cardPendulum = cards["double_pendulum"] | c.cardPendulum;
  c.cardCradle = cards["newtons_cradle"] | c.cardCradle;
  c.cardWater = cards["water"] | c.cardWater;
  c.cardBeer = cards["beer"] | c.cardBeer;
  c.cardPClock = cards["portrait_clock"] | c.cardPClock;
  c.cardHourglass = cards["hourglass"] | c.cardHourglass;
  c.cardLava = cards["lava_lamp"] | c.cardLava;
  c.cardTrivia = cards["trivia"] | c.cardTrivia;
  c.cardFacts = cards["facts_quotes"] | c.cardFacts;
  c.cardBillboard = cards["billboard"] | c.cardBillboard;
  c.cardFlyW = cards["fly_the_w"] | c.cardFlyW;
  c.cardIss = cards["iss_map"] | c.cardIss;
  c.cardPlanets = cards["planets"] | c.cardPlanets;
  c.cardNationalDay = cards["national_day"] | c.cardNationalDay;
  c.cardSurf = cards["surf"] | c.cardSurf;
  c.cardHackerNews = cards["hacker_news"] | c.cardHackerNews;
  if (d["billboard_years"].is<JsonArrayConst>()) {
    c.billboardYears.clear();
    for (JsonVariantConst y : d["billboard_years"].as<JsonArrayConst>()) c.billboardYears.push_back(y.as<int>());
  }
  c.surfName = d["surf"]["name"] | c.surfName.c_str();
  c.surfName.toUpperCase();
  c.surfLat = d["surf"]["lat"] | c.surfLat;
  c.surfLon = d["surf"]["lon"] | c.surfLon;
  c.orientation = d["motion"]["orientation"] | c.orientation.c_str();
  c.accelMirror = d["motion"]["mirror"] | c.accelMirror;
  c.accelTaps = d["motion"]["taps"] | c.accelTaps;
  c.flatSleep = d["motion"]["flat_sleep"] | c.flatSleep;
  c.pitchInvert = d["motion"]["pitch_invert"] | c.pitchInvert;
  c.ufc = d["ufc"]["show"] | c.ufc;
  c.ufcDays = d["ufc"]["days_ahead"] | c.ufcDays;

  c.yClientId = d["yahoo"]["client_id"] | "";
  c.ySecret = d["yahoo"]["client_secret"] | "";
  c.yRefresh = d["yahoo"]["refresh_token"] | "";
  c.yTeamKey = d["yahoo"]["team_key"] | "";
  c.yRedirect = d["yahoo"]["redirect_uri"] | "oob";
  return true;
}

String readConfigText(const char *path) {
  File f = SD.open(path);
  if (!f) return "{}";
  String s = f.readString();
  f.close();
  return s;
}

bool writeConfigText(const String &json, const char *path) {
  JsonDocument test;
  if (deserializeJson(test, json)) return false;  // refuse to save broken JSON
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  f.print(json);
  f.close();
  return true;
}
