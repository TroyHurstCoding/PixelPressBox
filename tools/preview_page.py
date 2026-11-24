#!/usr/bin/env python3
"""Preview the clock's phone page on a computer, without the clock.

    python3 tools/preview_page.py        then open http://localhost:8377

Serves the page out of src/webpage.h and fakes the clock's answers (settings from sdcard/config.json, kept in
memory only: nothing is written to disk)."""
import json
import os
import sys
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
STATE = {
    "cfg": open(os.path.join(ROOT, "sdcard", "config.json")).read(),
    "todo": [{"t": "Print the case", "d": False}, {"t": "Flash the clock", "d": True}],
    "wifi": ["Home", "Office"],
    "pin": "",
}


def page():
    src = open(os.path.join(ROOT, "src", "webpage.h")).read()
    a = src.index('R"HTML(') + 7
    return src[a:src.rindex(')HTML"')]


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def out(self, body, kind="application/json", code=200):
        data = body.encode() if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        q = {k: v[0] for k, v in urllib.parse.parse_qs(u.query, keep_blank_values=True).items()}
        p = u.path
        if p == "/":
            return self.out(page(), "text/html; charset=utf-8")
        if p == "/config":
            return self.out(STATE["cfg"])
        if p == "/status":
            return self.out(json.dumps({"line": "Busy light: auto (free now) | Next meeting: Standup at 9:30A",
                                        "ms": "Not signed in", "fw": "preview | Up to date (preview)"}))
        if p == "/cards":
            return self.out(json.dumps({"pinned": STATE["pin"], "cards": [
                {"k": "clock", "l": "Clock"}, {"k": "nfl:1", "l": "LV @ KC (nfl)"}, {"k": "fire", "l": "Fireplace"}]}))
        if p == "/sounds":
            return self.out(json.dumps(["td", "fg", "xp", "run", "score", "win", "chime", "focus", "startup"]))
        if p == "/motion":
            return self.out(json.dumps({"present": True, "cal": False, "mount": "panel", "orient": 0, "flat": False,
                                        "raw": [0.02, -0.97, 0.21], "g": [-0.02, 0.97, 0.21]}))
        if p == "/motion/cal":
            return self.out("Step %s saved (preview)" % q.get("s"), "text/plain")
        if p == "/wifi":
            return self.out(json.dumps({"current": STATE["wifi"][0] if STATE["wifi"] else "", "saved": STATE["wifi"]}))
        if p == "/wifi/del":
            STATE["wifi"] = [s for s in STATE["wifi"] if s != q.get("s")]
        elif p == "/pin":
            STATE["pin"] = "clock" if q.get("k") == "__current__" else q.get("k", "")
        elif p == "/todo":
            return self.out(json.dumps(STATE["todo"]))
        elif p == "/todo/add":
            STATE["todo"].append({"t": q.get("t", "")[:80], "d": False})
        elif p == "/todo/done":
            STATE["todo"][int(q["i"])]["d"] ^= True
        elif p == "/todo/del":
            STATE["todo"].pop(int(q["i"]))
        elif p == "/todo/clear":
            STATE["todo"] = [t for t in STATE["todo"] if not t["d"]]
        self.out("ok", "text/plain")

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", 0))).decode()
        q = {k: v[0] for k, v in urllib.parse.parse_qs(body, keep_blank_values=True).items()}
        if self.path == "/save":
            try:
                json.loads(q.get("cfg", ""))
            except ValueError:
                return self.out("Not saved: that isn't valid JSON.", "text/plain", 400)
            STATE["cfg"] = q["cfg"]
            return self.out("Saved. Restarting...", "text/plain")
        if self.path == "/wifi/add":
            if not q.get("s"):
                return self.out("bad", "text/plain", 400)
            STATE["wifi"] = [q["s"]] + [s for s in STATE["wifi"] if s != q["s"]]
        self.out("ok", "text/plain")


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8377
    print("Phone page preview: http://localhost:%d" % port)
    ThreadingHTTPServer(("127.0.0.1", port), H).serve_forever()
