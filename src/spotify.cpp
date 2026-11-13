#include "spotify.h"
#include <Preferences.h>
#include <base64.h>
#include "net.h"

static String accessToken;
static time_t expiresAt = 0;

bool spotifyConfigured() { return CFG.spClientId.length() && CFG.spSecret.length() && CFG.spRefresh.length(); }

static String refreshToken() {
  // Spotify may hand back a new refresh token; the newest one lives in flash.
  Preferences p;
  p.begin("spotify", true);
  String seed = p.getString("seed", ""), rt = p.getString("rt", "");
  p.end();
  return (seed != CFG.spRefresh || !rt.length()) ? CFG.spRefresh : rt;
}

static bool refresh() {
  JsonDocument d;
  int code = httpPostFormJson("https://accounts.spotify.com/api/token",
                              "grant_type=refresh_token&refresh_token=" + urlEncode(refreshToken()),
                              "Basic " + base64::encode(CFG.spClientId + ":" + CFG.spSecret), d);
  if (code != 200 || !d["access_token"].is<const char *>()) {
    Serial.printf("[spotify] refresh failed (%d)\n", code);
    return false;
  }
  accessToken = d["access_token"].as<String>();
  expiresAt = time(nullptr) + (d["expires_in"] | 3600) - 120;
  if (d["refresh_token"].is<const char *>()) {
    Preferences p;
    p.begin("spotify", false);
    p.putString("seed", CFG.spRefresh);
    p.putString("rt", d["refresh_token"].as<String>());
    p.end();
  }
  return true;
}

bool spotifyNowPlaying(NowPlaying &np) {
  if (!spotifyConfigured()) return false;
  if (!accessToken.length() || time(nullptr) >= expiresAt)
    if (!refresh()) return false;
  JsonDocument filter;
  filter["is_playing"] = true;
  filter["progress_ms"] = true;
  filter["item"]["name"] = true;
  filter["item"]["duration_ms"] = true;
  filter["item"]["artists"][0]["name"] = true;
  filter["item"]["show"]["name"] = true;  // podcasts
  JsonDocument d;
  int code = httpGetJson("https://api.spotify.com/v1/me/player/currently-playing?additional_types=episode", d,
                         &filter, accessToken.c_str());
  if (code == 401) accessToken = "";
  np.fetchedAt = millis();
  if (code == 204) {  // nothing playing
    np.playing = false;
    return true;
  }
  if (code != 200) return false;
  np.playing = d["is_playing"] | false;
  np.title = d["item"]["name"] | "";
  np.artist = d["item"]["artists"][0]["name"] | (d["item"]["show"]["name"] | "");
  np.progressMs = d["progress_ms"] | 0;
  np.durationMs = d["item"]["duration_ms"] | 0;
  return true;
}
