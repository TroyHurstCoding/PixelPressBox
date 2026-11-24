#include "wifinet.h"
#include <algorithm>

std::vector<int> wifiTryOrder(const std::vector<String> &saved, const std::vector<String> &seen,
                              const std::vector<int> &rssi) {
  std::vector<std::pair<int, int>> hits;  // (best rssi, index into saved)
  for (size_t i = 0; i < saved.size(); i++) {
    int best = -1000;
    for (size_t k = 0; k < seen.size() && k < rssi.size(); k++)
      if (seen[k] == saved[i] && rssi[k] > best) best = rssi[k];
    if (best > -1000) hits.push_back({best, (int)i});
  }
  std::stable_sort(hits.begin(), hits.end(), [](const std::pair<int, int> &a, const std::pair<int, int> &b) {
    return a.first > b.first;
  });
  std::vector<int> order;
  for (auto &h : hits) order.push_back(h.second);
  return order;
}

#ifndef PIXELPRESSBOX_SIM
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>

static std::vector<WifiNet> nets;
static const size_t MAX_NETS = 8;

static void save() {
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  for (auto &n : nets) {
    JsonObject o = a.add<JsonObject>();
    o["s"] = n.ssid;
    o["p"] = n.pass;
  }
  String out;
  serializeJson(d, out);
  Preferences p;
  p.begin("wifi", false);
  p.putString("list", out);
  p.end();
}

void wifiLoad() {
  nets.clear();
  Preferences p;
  p.begin("wifi", false);  // read-write so the first boot creates the store instead of logging an error
  String s = p.isKey("list") ? p.getString("list", "[]") : String("[]");
  p.end();
  JsonDocument d;
  if (deserializeJson(d, s)) return;
  for (JsonObjectConst o : d.as<JsonArrayConst>()) {
    WifiNet n;
    n.ssid = o["s"] | "";
    n.pass = o["p"] | "";
    if (n.ssid.length() && nets.size() < MAX_NETS) nets.push_back(n);
  }
}

bool wifiRemember(const String &ssid, const String &pass) {
  if (!ssid.length() || ssid.length() > 32 || pass.length() > 64) return false;
  if (!nets.empty() && nets[0].ssid == ssid && nets[0].pass == pass) return true;  // already newest: no flash write
  for (size_t i = 0; i < nets.size(); i++)
    if (nets[i].ssid == ssid) {
      nets.erase(nets.begin() + i);
      break;
    }
  nets.insert(nets.begin(), {ssid, pass});
  if (nets.size() > MAX_NETS) nets.pop_back();  // the one joined longest ago drops off
  save();
  return true;
}

bool wifiForget(const String &ssid) {
  for (size_t i = 0; i < nets.size(); i++)
    if (nets[i].ssid == ssid) {
      nets.erase(nets.begin() + i);
      save();
      return true;
    }
  return false;
}

const std::vector<WifiNet> &wifiSaved() { return nets; }

bool wifiConnectSaved(uint32_t perNetworkMs) {
  if (nets.empty()) return false;
  WiFi.mode(WIFI_STA);
  int found = WiFi.scanNetworks();
  std::vector<String> saved, seen;
  std::vector<int> rssi;
  for (auto &n : nets) saved.push_back(n.ssid);
  for (int i = 0; i < found; i++) {
    seen.push_back(WiFi.SSID(i));
    rssi.push_back(WiFi.RSSI(i));
  }
  WiFi.scanDelete();
  for (int idx : wifiTryOrder(saved, seen, rssi)) {
    Serial.printf("[wifi] trying %s\n", nets[idx].ssid.c_str());
    WiFi.disconnect();
    delay(100);
    WiFi.begin(nets[idx].ssid.c_str(), nets[idx].pass.c_str());
    uint32_t t0 = millis();
    while (millis() - t0 < perNetworkMs) {
      if (WiFi.status() == WL_CONNECTED) return true;
      delay(200);
    }
  }
  return false;
}

String wifiListJson() {
  JsonDocument d;
  d["current"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String();
  JsonArray a = d["saved"].to<JsonArray>();
  for (auto &n : nets) a.add(n.ssid);
  String out;
  serializeJson(d, out);
  return out;
}
#endif
