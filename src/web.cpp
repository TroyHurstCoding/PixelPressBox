#include "web.h"
#include <ESPmDNS.h>
#include <SD.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <Update.h>
#include "render.h"
#include "autoupdate.h"
#include <ArduinoJson.h>
#include "msgraph.h"
#include "settings.h"
#include "todo.h"
#include "webpage_gz.h"  // src/webpage.h, gzipped at build time by tools/gzip_webpage.py
#include "wifinet.h"
#include "motion.h"

static WebServer server(80);
volatile bool webWantsRestart = false;
String webTestSound;
volatile bool webNext = false;
volatile bool webToggleFav = false;
volatile int webBusyMode = -1;
volatile int webFocusCmd = -1;
volatile bool webMsLogin = false;
volatile bool webDeskChanged = false;
String webStatusLine;
String webPinRequest;
volatile bool webPinChanged = false;
volatile bool webCheckUpdate = false;
volatile int webGameCmd = -2;
String webCardsJson = "{\"pinned\":\"\",\"cards\":[]}";

static void handleRoot() {
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", (const char *)WEB_PAGE_GZ, sizeof(WEB_PAGE_GZ));
}

// names of the sounds on the card, without extensions: ["td","fg","LV_td",...]
static String soundsJson() {
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  File dir = SD.open("/sounds");
  if (dir) {
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      String n = f.name();
      int slash = n.lastIndexOf('/');
      if (slash >= 0) n = n.substring(slash + 1);
      int dot = n.lastIndexOf('.');
      if (!f.isDirectory() && !n.startsWith(".") && dot > 0) a.add(n.substring(0, dot));
      f.close();
    }
  }
  String out;
  serializeJson(d, out);
  return out;
}

static void handleSave() {
  if (!server.hasArg("cfg")) return server.send(400, "text/plain", "missing cfg");
  if (!writeConfigText(server.arg("cfg"))) return server.send(400, "text/plain", "Not saved: that isn't valid JSON.");
  server.send(200, "text/plain", "Saved. Restarting...");
  webWantsRestart = true;
}

static HTTPUpdateServer updater;

void webBegin() {
  MDNS.begin("pixelpressbox");
  // Firmware updates over WiFi: http://pixelpressbox.local/update (user "admin", password from config).
  // The new image goes into the spare app slot, is checked, then the board restarts into it.
  updater.setup(&server, "/update", "admin", CFG.updatePassword);
  Update.onProgress([](size_t done, size_t) {
    otaBytes = done;
    if (!otaOnMainTask()) return;  // automatic update: the display loop draws the progress
    static size_t last = 0;
    if (done - last < 64 * 1024 && done > last) return;  // phone-page upload: redraw every 64 KB
    last = done;
    drawMessage("UPDATING", String((unsigned)(done / 1024)) + " KB", 0x40A0FF);
  });
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/config", HTTP_GET, [] { server.send(200, "application/json", readConfigText()); });
  server.on("/sounds", HTTP_GET, [] { server.send(200, "application/json", soundsJson()); });
  // remembered WiFi networks (names only; passwords never leave the clock)
  // motion sensor: live readings for the phone page, and the two-step mounting calibration
  server.on("/motion", HTTP_GET, [] {
    const Motion &m = motion();
    JsonDocument d;
    d["present"] = m.present;
    d["cal"] = m.calibrated;
    d["mount"] = CFG.accelMount;
    d["orient"] = m.orient;
    d["flat"] = m.flat;
    float r[3];
    motionRaw(r);
    JsonArray raw = d["raw"].to<JsonArray>(), g = d["g"].to<JsonArray>();
    for (float v : r) raw.add(roundf(v * 100) / 100);
    for (float v : {m.gx, m.gy, m.gz}) g.add(roundf(v * 100) / 100);
    String out;
    serializeJson(d, out);
    server.send(200, "application/json", out);
  });
  server.on("/motion/cal", HTTP_GET, [] {
    server.send(200, "text/plain", motionCalibrate(constrain(server.arg("s").toInt(), 0, 2)));
    webDeskChanged = true;  // redraw with the new orientation
  });
  server.on("/wifi", HTTP_GET, [] { server.send(200, "application/json", wifiListJson()); });
  server.on("/wifi/add", HTTP_POST, [] {
    bool ok = wifiRemember(server.arg("s"), server.arg("p"));
    server.send(ok ? 200 : 400, "text/plain", ok ? "ok" : "bad network name or password");
  });
  server.on("/wifi/del", HTTP_GET, [] {
    wifiForget(server.arg("s"));
    server.send(200, "text/plain", "ok");
  });
  server.on("/test", HTTP_GET, [] {
    webTestSound = server.arg("s");
    server.send(200, "text/plain", "ok");
  });
  server.on("/next", HTTP_GET, [] {
    webNext = true;
    server.send(200, "text/plain", "ok");
  });
  server.on("/favs", HTTP_GET, [] {
    webToggleFav = true;
    server.send(200, "text/plain", "ok");
  });
  server.on("/game", HTTP_GET, [] {
    int g = server.arg("g").toInt();
    webGameCmd = (g == 99 || (g >= -1 && g <= 6)) ? g : -2;
    server.send(200, "text/plain", "ok");
  });
  server.on("/focus", HTTP_GET, [] {
    webFocusCmd = constrain(server.arg("m").toInt(), 0, 180);
    server.send(200, "text/plain", "ok");
  });
  server.on("/busy", HTTP_GET, [] {
    webBusyMode = constrain(server.arg("m").toInt(), 0, 2);
    server.send(200, "text/plain", "ok");
  });
  server.on("/ms/login", HTTP_GET, [] {
    webMsLogin = true;
    server.send(200, "text/plain", "ok");
  });
  server.on("/ms/logout", HTTP_GET, [] {
    msSignOut();
    todoSetMicrosoft({});
    server.send(200, "text/plain", "ok");
  });
  server.on("/status", HTTP_GET, [] {
    JsonDocument d;
    d["line"] = webStatusLine;
    d["ms"] = msStatus();
    d["fw"] = String(FW_VERSION) + " | " + lastUpdateStatus();
    if (otaBusy) d["ota"] = (uint32_t)otaBytes;
    String out;
    serializeJson(d, out);
    server.send(200, "application/json", out);
  });
  server.on("/checkupdate", HTTP_GET, [] {
    updateCheckQueued();
    webCheckUpdate = true;  // picked up by the network task within a second (it may restart the board)
    server.send(200, "text/plain", "checking");
  });
  server.on("/cards", HTTP_GET, [] { server.send(200, "application/json", webCardsJson); });
  server.on("/pin", HTTP_GET, [] {  // /pin?k=clock   /pin?k=   (empty = rotate)   /pin?k=__current__
    webPinRequest = server.arg("k");
    webPinChanged = true;
    server.send(200, "text/plain", "ok");
  });
  server.on("/todo", HTTP_GET, [] { server.send(200, "application/json", todoJson()); });
  server.on("/todo/add", HTTP_GET, [] {
    webDeskChanged = todoAdd(server.arg("t"));
    server.send(200, "text/plain", "ok");
  });
  server.on("/todo/done", HTTP_GET, [] {
    webDeskChanged = todoToggle(server.arg("i").toInt());
    server.send(200, "text/plain", "ok");
  });
  server.on("/todo/del", HTTP_GET, [] {
    webDeskChanged = todoRemove(server.arg("i").toInt());
    server.send(200, "text/plain", "ok");
  });
  server.on("/todo/clear", HTTP_GET, [] {
    webDeskChanged = todoClearDone();
    server.send(200, "text/plain", "ok");
  });
  server.on("/restart", HTTP_GET, [] {
    server.send(200, "text/plain", "restarting");
    webWantsRestart = true;
  });
  server.begin();
  MDNS.addService("http", "tcp", 80);
}

void webLoop() { server.handleClient(); }
