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
String webCardsJson = "{\"pinned\":\"\",\"cards\":[]}";

static const char PAGE_HEAD[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1"><title>PixelPressbox</title>
<style>body{font-family:-apple-system,system-ui,sans-serif;background:#111;color:#eee;margin:0;padding:16px;max-width:720px}
h1{font-size:22px;margin:0 0 4px}p{color:#aaa;margin:4px 0 14px}
textarea{width:100%;height:55vh;background:#000;color:#9f9;font:13px ui-monospace,Menlo,monospace;border:1px solid #333;border-radius:8px;padding:10px;box-sizing:border-box}
button{background:#2a6;color:#fff;border:0;border-radius:8px;padding:10px 14px;font-size:15px;margin:8px 6px 0 0}
button.alt{background:#333}.row{margin-top:14px}h2{font-size:17px;margin:22px 0 4px}
#st{color:#9cf}input[type=text]{background:#000;color:#eee;border:1px solid #333;border-radius:8px;padding:10px;font-size:15px;width:65%}
ul{list-style:none;padding:0}li{padding:7px 0;border-bottom:1px solid #222;display:flex;gap:10px;align-items:center}
li.d span{text-decoration:line-through;color:#666}li span{flex:1}li button{margin:0;padding:4px 10px}</style></head><body>
<h1>PixelPressbox</h1><p id='st'></p>
<div><button onclick="go('/next')">&#9654; Next</button><button class='alt' onclick="go('/favs')">&#9733; Favorites only on/off</button></div>
<h2>Screen</h2>
<div><select id=pin onchange="go('/pin?k='+encodeURIComponent(this.value))" style="background:#000;color:#eee;border:1px solid #333;border-radius:8px;padding:10px;font-size:15px;width:100%"></select></div>
<div><button onclick="go('/pin?k=__current__')">&#128204; Keep this card on</button><button class='alt' onclick="go('/pin?k=')">&#8635; Rotate all</button></div>
<h2>Focus timer</h2>
<div><button onclick="go('/focus?m=25')">25 min</button><button onclick="go('/focus?m=50')">50 min</button><button class='alt' onclick="go('/focus?m=0')">Stop</button></div>
<h2>Busy light</h2>
<div><button class='alt' onclick="go('/busy?m=0')">Auto (calendar)</button><button style='background:#c33' onclick="go('/busy?m=1')">Busy</button><button class='alt' onclick="go('/busy?m=2')">Free</button></div>
<h2>To-do</h2>
<form onsubmit="add();return false"><input type=text id=nt placeholder='Add a to-do'><button>Add</button></form>
<ul id=tl></ul><button class='alt' onclick="go('/todo/clear').then(load)">Clear done</button>
<h2>Outlook</h2><p id=ms></p>
<div><button onclick="go('/ms/login')">Sign in to Outlook</button><button class='alt' onclick="go('/ms/logout')">Sign out</button></div>
<script>
function go(u){return fetch(u).then(()=>setTimeout(status,600))}
function esc(t){return t.replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function load(){fetch('/todo').then(r=>r.json()).then(l=>{document.getElementById('tl').innerHTML=l.map((x,i)=>
"<li class='"+(x.d?'d':'')+"'><input type=checkbox "+(x.d?'checked':'')+" onchange=\"go('/todo/done?i="+i+"').then(load)\"><span>"+esc(x.t)+
"</span><button class=alt onclick=\"go('/todo/del?i="+i+"').then(load)\">&times;</button></li>").join('')})}
function add(){let e=document.getElementById('nt');if(!e.value)return;fetch('/todo/add?t='+encodeURIComponent(e.value)).then(()=>{e.value='';load()})}
function status(){fetch('/status').then(r=>r.json()).then(s=>{document.getElementById('st').textContent=s.line;document.getElementById('ms').textContent=s.ms;if(s.fw)document.getElementById('fw').textContent='Firmware '+s.fw});
fetch('/cards').then(r=>r.json()).then(c=>{let p=document.getElementById('pin');if(document.activeElement===p)return;
p.innerHTML="<option value=''>Rotate through all cards</option>"+c.cards.map(x=>"<option value='"+esc(x.k)+"'"+(x.k==c.pinned?" selected":"")+">Show only: "+esc(x.l)+"</option>").join('')})}
function checkUpd(){let b=document.getElementById('cu'),f=document.getElementById('fw');b.disabled=true;b.textContent='Checking...';
f.textContent='Asking GitHub for a newer version...';let n=0;
fetch('/checkupdate').catch(()=>{});let t=setInterval(()=>{n++;fetch('/status').then(r=>r.json()).then(s=>{
let m=s.fw;if(s.ota)m+=' ('+Math.round(s.ota/1024)+' KB)';f.textContent='Firmware '+m;
if(/Up to date|failed|off/.test(s.fw)){clearInterval(t);b.disabled=false;b.textContent='Check for update'}
}).catch(()=>{if(n>3)f.textContent='Restarting with the new firmware... reload in a moment'});if(n>150){clearInterval(t);b.disabled=false;b.textContent='Check for update'}},2000)}
load();status();setInterval(status,5000);
</script>
<h2>Settings</h2>
<p>Edit config.json, then Save &amp; restart.</p>)HTML";

static void handleRoot() {
  String html = FPSTR(PAGE_HEAD);
  String cfg = readConfigText();
  cfg.replace("&", "&amp;");
  cfg.replace("<", "&lt;");
  html += "<form method='POST' action='/save'><textarea name='cfg' spellcheck='false'>" + cfg +
          "</textarea><button>Save &amp; restart</button></form>"
          "<div class='row'><b>Test sounds</b><br>";
  File dir = SD.open("/sounds");
  if (dir) {
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      String n = f.name();
      int dot = n.lastIndexOf('.');
      String base = dot > 0 ? n.substring(0, dot) : n;
      html += "<button class='alt' onclick=\"fetch('/test?s=" + base + "')\">" + base + "</button>";
    }
  }
  html += "</div><div class='row'><button class='alt' onclick=\"fetch('/restart')\">Restart</button>"
          "<button class='alt' id=cu onclick='checkUpd()'>Check for update</button>"
          "<button class='alt' onclick=\"location.href='/update'\">Upload firmware</button></div>"
          "<p style='color:#777' id=fw>Firmware " FW_VERSION "</p></body></html>";
  server.send(200, "text/html", html);
}

static void handleSave() {
  if (!server.hasArg("cfg")) return server.send(400, "text/plain", "missing cfg");
  if (!writeConfigText(server.arg("cfg"))) return server.send(400, "text/plain", "Not saved: that isn't valid JSON.");
  server.send(200, "text/html", "<meta http-equiv='refresh' content='8;url=/'>Saved. Restarting...");
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
