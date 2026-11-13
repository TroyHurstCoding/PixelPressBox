#include "yahoo.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <base64.h>
#include "net.h"

static String accessToken;
static time_t expiresAt = 0;
static Preferences prefs;

bool yahooConfigured() { return CFG.yClientId.length() && CFG.yRefresh.length() && CFG.yTeamKey.length(); }

static String refreshToken() {
  // Yahoo rotates refresh tokens; the newest one is kept in flash (NVS).
  prefs.begin("yahoo", true);
  String seed = prefs.getString("seed", "");
  String rt = prefs.getString("rt", "");
  prefs.end();
  // if config.json got a new token (re-auth), it wins
  if (seed != CFG.yRefresh || !rt.length()) return CFG.yRefresh;
  return rt;
}

static void saveRefresh(const String &rt) {
  prefs.begin("yahoo", false);
  prefs.putString("seed", CFG.yRefresh);
  prefs.putString("rt", rt);
  prefs.end();
}

static bool refresh(time_t now) {
  String auth = "Basic " + base64::encode(CFG.yClientId + ":" + CFG.ySecret);
  String body = "grant_type=refresh_token&redirect_uri=" + urlEncode(CFG.yRedirect) +
                "&refresh_token=" + urlEncode(refreshToken());
  JsonDocument d;
  int code = httpPostFormJson("https://api.login.yahoo.com/oauth2/get_token", body, auth, d);
  if (code != 200 || !d["access_token"].is<const char *>()) {
    Serial.printf("[yahoo] token refresh failed (%d)\n", code);
    return false;
  }
  accessToken = d["access_token"].as<String>();
  expiresAt = now + (d["expires_in"] | 3600) - 120;
  if (d["refresh_token"].is<const char *>()) saveRefresh(d["refresh_token"].as<String>());
  return true;
}

// Yahoo nests a team as an array of single-key objects; merge into one view.
struct TeamInfo {
  String key, name;
  float pts = 0, proj = 0;
  bool hasPts = false;
};

static void absorb(JsonVariantConst v, TeamInfo &t) {
  if (v.is<JsonArrayConst>()) {
    for (JsonVariantConst x : v.as<JsonArrayConst>()) absorb(x, t);
  } else if (v.is<JsonObjectConst>()) {
    JsonObjectConst o = v.as<JsonObjectConst>();
    if (o["team_key"].is<const char *>() && !t.key.length()) t.key = o["team_key"].as<String>();
    if (o["name"].is<const char *>() && !t.name.length()) t.name = o["name"].as<String>();
    if (!o["team_points"].isNull()) {
      t.pts = atof(o["team_points"]["total"] | "0");
      t.hasPts = true;
    }
    if (!o["team_projected_points"].isNull()) t.proj = atof(o["team_projected_points"]["total"] | "0");
  }
}

static void findTeams(JsonVariantConst v, std::vector<TeamInfo> &acc) {
  if (v.is<JsonObjectConst>()) {
    for (JsonPairConst kv : v.as<JsonObjectConst>()) {
      if (strcmp(kv.key().c_str(), "team") == 0 && kv.value().is<JsonArrayConst>()) {
        TeamInfo t;
        // only the team's own fields, not nested matchups
        for (JsonVariantConst part : kv.value().as<JsonArrayConst>())
          if (part.is<JsonArrayConst>() || part["team_points"].is<JsonObjectConst>() ||
              part["team_projected_points"].is<JsonObjectConst>())
            absorb(part, t);
        if (t.key.length()) acc.push_back(t);
      }
      findTeams(kv.value(), acc);  // keep digging: matchup teams are nested inside
    }
  } else if (v.is<JsonArrayConst>()) {
    for (JsonVariantConst x : v.as<JsonArrayConst>()) findTeams(x, acc);
  }
}

bool yahooMatchup(time_t now, Card &out) {
  if (!yahooConfigured()) return false;
  if (!accessToken.length() || now >= expiresAt)
    if (!refresh(now)) return false;
  String url = "https://fantasysports.yahooapis.com/fantasy/v2/team/" + CFG.yTeamKey +
               "/matchups;weeks=current?format=json";
  JsonDocument d;
  if (httpGetJson(url, d, nullptr, accessToken.c_str()) != 200) return false;
  std::vector<TeamInfo> teams;
  findTeams(d.as<JsonVariantConst>(), teams);
  const TeamInfo *me = nullptr, *opp = nullptr;
  for (auto &t : teams) {
    if (!t.hasPts) continue;
    if (t.key == CFG.yTeamKey && !me) me = &t;
    else if (t.key != CFG.yTeamKey && !opp) opp = &t;
  }
  if (!me) return false;
  out = Card();
  out.kind = CARD_YAHOO;
  out.key = "yahoo";
  out.pri = PRI_FANTASY + 10;
  out.dwell = 6;
  out.meName = me->name;
  out.mePts = me->pts;
  out.meProj = me->proj;
  out.oppName = opp ? opp->name : "OPP";
  out.oppPts = opp ? opp->pts : 0;
  out.oppProj = opp ? opp->proj : 0;
  return true;
}
