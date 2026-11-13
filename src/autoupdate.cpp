#include "autoupdate.h"
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include "model.h"
#include "net.h"
#include "version.h"

static String status = "Not checked yet";
volatile bool otaBusy = false;
volatile uint32_t otaBytes = 0;
static TaskHandle_t mainTask = nullptr;
void otaSetMainTask() { mainTask = xTaskGetCurrentTaskHandle(); }
bool otaOnMainTask() { return xTaskGetCurrentTaskHandle() == mainTask; }
static int lastCheckDay = -1;

String lastUpdateStatus() { return status; }
void updateCheckQueued() { status = "Check queued..."; }

static bool install(const String &url, size_t size, const String &md5) {
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();
  HTTPClient http;
  http.setTimeout(30000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("PixelPressbox/" FW_VERSION);
  if (!http.begin(client, url)) {
    status = "Update failed: bad firmware URL";
    return false;
  }
  int code = http.GET();
  if (code != 200) {
    status = "Update failed: download HTTP " + String(code);
    http.end();
    return false;
  }
  int len = http.getSize();
  if (len > 0) size = len;
  if (!Update.begin(size ? size : UPDATE_SIZE_UNKNOWN)) {
    status = "Update failed: " + String(Update.errorString());
    http.end();
    return false;
  }
  if (md5.length() == 32) Update.setMD5(md5.c_str());  // Update.end() rejects the image if it doesn't match
  otaBytes = 0;
  otaBusy = true;
  size_t written = Update.writeStream(*http.getStreamPtr());
  http.end();
  if (!Update.end(true) || (size && written != size)) {
    status = "Update failed: " + String(Update.errorString()) + " (" + String((unsigned)written) + " bytes)";
    otaBusy = false;
    return false;
  }
  return true;
}

String checkForUpdate() {
  status = "Checking GitHub...";
  if (!CFG.autoUpdateManifest.length()) return status = "Auto-update off (no manifest URL)";
  JsonDocument m;
  int code = httpGetJson(CFG.autoUpdateManifest + "?t=" + String((uint32_t)time(nullptr)), m);  // skip caches
  if (code != 200) return status = "Update check failed: HTTP " + String(code);
  String ver = m["version"] | "", bin = m["bin"] | "", md5 = m["md5"] | "";
  size_t size = m["size"] | 0;
  if (!ver.length() || !bin.length()) return status = "Update check failed: bad manifest";
  if (ver == FW_VERSION) return status = "Up to date (" + ver + ")";
  Serial.printf("[update] %s -> %s\n", FW_VERSION, ver.c_str());
  status = "Downloading " + ver;
  if (install(bin, size, md5)) {
    status = "Installed " + ver + ", restarting";
    delay(1500);
    ESP.restart();
  }
  return status;
}

void autoUpdateTick(time_t now) {
  if (!CFG.autoUpdate || now < 1700000000) return;
  struct tm t;
  localtime_r(&now, &t);
  if (t.tm_hour != CFG.autoUpdateHour || t.tm_mday == lastCheckDay) return;
  lastCheckDay = t.tm_mday;
  checkForUpdate();
}
