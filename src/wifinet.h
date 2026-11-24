// wifinet.h - remembers every WiFi network the clock has joined (up to 8) and connects to whichever one is in
// range, strongest first. The list lives in the board's own flash (Preferences), not on the SD card.
#pragma once
#include <Arduino.h>
#include <vector>

struct WifiNet {
  String ssid, pass;
};

void wifiLoad();                                        // read the saved list
bool wifiRemember(const String &ssid, const String &pass);  // add or update (newest first); false if ssid is empty
bool wifiForget(const String &ssid);
const std::vector<WifiNet> &wifiSaved();
// Scan, then try each saved network that's in range (strongest first). True once connected.
bool wifiConnectSaved(uint32_t perNetworkMs = 12000);
String wifiListJson();                                  // {"current":"Home","saved":["Home","Office"]} - no passwords

// ---- pure helpers (unit-tested)
// order in which to try the saved networks: only those seen in the scan, strongest signal first
std::vector<int> wifiTryOrder(const std::vector<String> &saved, const std::vector<String> &seen,
                              const std::vector<int> &rssi);
