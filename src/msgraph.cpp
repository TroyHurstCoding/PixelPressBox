#include "msgraph.h"
#include <Preferences.h>
#include "desk.h"
#include "net.h"

static String scopes() {
  String result = "offline_access%20Calendars.Read%20Mail.ReadBasic%20Presence.Read";
  if (CFG.msTodo) result += "%20Tasks.Read";
  return result;
}
static String accessToken, userCode, verifyUrl, deviceCode, lastError;
static time_t accessExpires = 0;
static uint32_t loginDeadline = 0, nextPollAt = 0, pollEvery = 5000;
static bool pending = false;

static String authority() { return "https://login.microsoftonline.com/" + CFG.msTenant + "/oauth2/v2.0/"; }

static String loadRefresh() {
  Preferences p;
  p.begin("ms", true);
  String rt = p.getString("rt", "");
  p.end();
  return rt;
}

static void saveRefresh(const String &rt) {
  Preferences p;
  p.begin("ms", false);
  if (rt.length()) p.putString("rt", rt);
  else p.remove("rt");
  p.end();
}

bool msConfigured() { return CFG.msClientId.length() > 0; }
bool msSignedIn() { return msConfigured() && loadRefresh().length() > 0; }
bool msLoginPending() { return pending; }
String msUserCode() { return userCode; }
String msVerifyUrl() { return verifyUrl; }

String msStatus() {
  if (!msConfigured()) return "Not set up: add outlook.client_id to config.json";
  if (pending) return "Go to " + verifyUrl + " and enter code " + userCode;
  if (msSignedIn()) return lastError.length() ? "Signed in (last error: " + lastError + ")" : "Signed in";
  return lastError.length() ? "Not signed in (" + lastError + ")" : "Not signed in";
}

void msSignOut() {
  saveRefresh("");
  accessToken = "";
  pending = false;
}

bool msStartLogin() {
  if (!msConfigured()) return false;
  JsonDocument d;
  int code = httpPostFormJson(authority() + "devicecode", "client_id=" + CFG.msClientId + "&scope=" + scopes(), "", d);
  if (code != 200 || !d["user_code"].is<const char *>()) {
    lastError = d["error"] | "";
    if (!lastError.length()) {
      if (code == -2) lastError = "Microsoft response was not valid JSON";
      else if (code == -3) lastError = "Microsoft response could not be read";
      else if (code < 0) lastError = "Could not connect to Microsoft";
      else lastError = "Microsoft did not return a device code";
    }
    lastError += " (" + String(code < 0 ? "network " : "HTTP ") + String(code) + ")";
    long aadsts = d["error_codes"][0] | 0L;
    if (aadsts > 0) lastError += " AADSTS" + String(aadsts);
    Serial.printf("[ms] devicecode %d %s\n", code, lastError.c_str());
    return false;
  }
  userCode = d["user_code"].as<String>();
  verifyUrl = d["verification_uri"] | "https://microsoft.com/devicelogin";
  deviceCode = d["device_code"].as<String>();
  pollEvery = (d["interval"] | 5) * 1000UL;
  loginDeadline = millis() + (d["expires_in"] | 900) * 1000UL;
  nextPollAt = millis() + pollEvery;
  pending = true;
  lastError = "";
  return true;
}

static bool takeTokens(JsonDocument &d) {
  if (!d["access_token"].is<const char *>()) return false;
  accessToken = d["access_token"].as<String>();
  accessExpires = time(nullptr) + (d["expires_in"] | 3600) - 120;
  if (d["refresh_token"].is<const char *>()) saveRefresh(d["refresh_token"].as<String>());  // they rotate
  return true;
}

int msPollLogin() {
  if (!pending) return msSignedIn() ? 1 : -1;
  if ((int32_t)(millis() - loginDeadline) > 0) {
    pending = false;
    lastError = "code expired";
    return -1;
  }
  if ((int32_t)(millis() - nextPollAt) < 0) return 0;
  nextPollAt = millis() + pollEvery;
  JsonDocument d;
  httpPostFormJson(authority() + "token",
                   "grant_type=urn:ietf:params:oauth:grant-type:device_code&client_id=" + CFG.msClientId +
                       "&device_code=" + urlEncode(deviceCode),
                   "", d);
  if (takeTokens(d)) {
    pending = false;
    return 1;
  }
  String err = d["error"] | "";
  if (err == "authorization_pending") return 0;
  if (err == "slow_down") {
    pollEvery += 5000;
    return 0;
  }
  pending = false;
  lastError = err.length() ? err : "sign-in failed";
  return -1;
}

static bool ensureToken() {
  if (accessToken.length() && time(nullptr) < accessExpires) return true;
  String rt = loadRefresh();
  if (!rt.length()) return false;
  JsonDocument d;
  int code = httpPostFormJson(authority() + "token",
                              "grant_type=refresh_token&client_id=" + CFG.msClientId + "&scope=" + scopes() +
                                  "&refresh_token=" + urlEncode(rt),
                              "", d);
  if (takeTokens(d)) return true;
  lastError = d["error"] | "token refresh failed";
  Serial.printf("[ms] refresh %d %s\n", code, lastError.c_str());
  if (lastError == "invalid_grant") saveRefresh("");  // revoked/expired: sign in again
  return false;
}

bool msFetchCalendar(time_t now, std::vector<Meeting> &out) {
  if (!ensureToken()) return false;
  String url = "https://graph.microsoft.com/v1.0/me/calendarView?startDateTime=" + isoUtc(now - 3600) +
               "&endDateTime=" + isoUtc(now + 24 * 3600) +
               "&$select=subject,start,end,showAs,isAllDay,isCancelled,responseStatus"
               "&$orderby=start/dateTime&$top=40";
  JsonDocument filter;
  JsonObject f = filter["value"].add<JsonObject>();
  f["subject"] = true;
  f["start"]["dateTime"] = true;
  f["end"]["dateTime"] = true;
  f["showAs"] = true;
  f["isAllDay"] = true;
  f["isCancelled"] = true;
  f["responseStatus"]["response"] = true;
  JsonDocument d;
  int code = httpGetJson(url, d, &filter, accessToken.c_str());
  if (code == 401) accessToken = "";
  if (code != 200) {
    lastError = "calendar HTTP " + String(code);
    return false;
  }
  out.clear();
  for (JsonObjectConst e : d["value"].as<JsonArrayConst>()) {
    if (e["isCancelled"] | false) continue;
    String resp = e["responseStatus"]["response"] | "";
    if (resp == "declined") continue;
    Meeting m;
    m.subject = e["subject"] | "(no subject)";
    m.start = parseIsoUtc(e["start"]["dateTime"] | "");  // Graph returns UTC unless asked otherwise
    m.end = parseIsoUtc(e["end"]["dateTime"] | "");
    m.showAs = e["showAs"] | "busy";
    m.allDay = e["isAllDay"] | false;
    if (m.start && m.end) out.push_back(m);
  }
  lastError = "";
  return true;
}

bool msFetchFlagged(std::vector<String> &out) {
  if (!CFG.msFlaggedMail || !ensureToken()) return false;
  String url = "https://graph.microsoft.com/v1.0/me/messages?$filter=flag/flagStatus%20eq%20%27flagged%27"
               "&$select=subject&$top=10";
  JsonDocument filter;
  filter["value"][0]["subject"] = true;
  JsonDocument d;
  int code = httpGetJson(url, d, &filter, accessToken.c_str());
  if (code == 401) accessToken = "";
  if (code != 200) return false;
  out.clear();
  for (JsonObjectConst m : d["value"].as<JsonArrayConst>()) out.push_back(m["subject"] | "(no subject)");
  return true;
}

String msTaskToken() { return CFG.msTodo && ensureToken() ? accessToken : String(); }
bool msTaskTokenCurrent(const String &token) { return accessToken == token; }

bool msFetchTasks(const String &token, std::vector<String> &out) {
  static const char *BASE = "https://graph.microsoft.com/v1.0/me/todo/lists";
  constexpr size_t maxLists = 8;
  constexpr size_t maxTasks = 30;
  constexpr int maxPages = 4;

  JsonDocument filter;
  filter["@odata.nextLink"] = true;
  filter["value"][0]["id"] = true;
  std::vector<String> lists;
  String url = String(BASE) + "?$select=id&$top=20";
  for (int page = 0; page < maxPages && url.length() && lists.size() < maxLists; page++) {
    if (!url.startsWith(String(BASE) + "?")) return false;
    JsonDocument d;
    int code = httpGetJson(url, d, &filter, token.c_str());
    if (code != 200 || !d["value"].is<JsonArrayConst>()) {
      Serial.printf("[ms] To Do lists failed (%d)\n", code);
      return false;
    }
    for (JsonObjectConst list : d["value"].as<JsonArrayConst>()) {
      const char *id = list["id"] | "";
      if (*id) lists.emplace_back(id);
      if (lists.size() >= maxLists) break;
    }
    url = d["@odata.nextLink"] | "";
  }

  filter.clear();
  filter["@odata.nextLink"] = true;
  filter["value"][0]["title"] = true;
  filter["value"][0]["status"] = true;
  std::vector<String> tasks;
  for (const String &id : lists) {
    String endpoint = String(BASE) + "/" + urlEncode(id) + "/tasks?";
    url = endpoint + "$select=title,status&$top=30";
    for (int page = 0; page < maxPages && url.length() && tasks.size() < maxTasks; page++) {
      if (!url.startsWith(endpoint)) return false;
      JsonDocument d;
      int code = httpGetJson(url, d, &filter, token.c_str());
      if (code != 200 || !d["value"].is<JsonArrayConst>()) {
        Serial.printf("[ms] To Do tasks failed (%d)\n", code);
        return false;
      }
      for (JsonObjectConst task : d["value"].as<JsonArrayConst>()) {
        String status = task["status"] | "";
        String title = task["title"] | "";
        title.trim();
        if (status.length() && status != "completed" && title.length()) tasks.push_back(title.substring(0, 80));
        if (tasks.size() >= maxTasks) break;
      }
      url = d["@odata.nextLink"] | "";
    }
    if (tasks.size() >= maxTasks) break;
  }
  out = std::move(tasks);
  return true;
}

bool msFetchPresence(String &availability, String &activity) {
  if (!ensureToken()) return false;
  JsonDocument d;
  int code = httpGetJson("https://graph.microsoft.com/v1.0/me/presence", d, nullptr, accessToken.c_str());
  if (code == 401) accessToken = "";
  if (code != 200) return false;
  availability = d["availability"] | "PresenceUnknown";
  activity = d["activity"] | "";
  return true;
}
