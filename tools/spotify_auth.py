#!/usr/bin/env python3
"""One-time Spotify sign-in for PixelPressbox's "now playing" card. Run it on your Mac.

1. Go to https://developer.spotify.com/dashboard and create an app
   (Spotify requires the app owner to have Premium for developer apps).
   - Redirect URI:  http://127.0.0.1:8888/callback
   - APIs used:     Web API
2. Run:  python3 tools/spotify_auth.py
   Paste the app's Client ID and Client secret, approve in the browser that opens.
3. Copy the printed "spotify" block into config.json on the SD card (or the phone page).

Uses only the Python standard library.
"""
import base64
import http.server
import json
import secrets
import urllib.parse
import urllib.request
import webbrowser

REDIRECT = "http://127.0.0.1:8888/callback"
SCOPES = "user-read-currently-playing user-read-playback-state"


def main():
    cid = input("Spotify Client ID: ").strip()
    secret = input("Spotify Client secret: ").strip()
    state = secrets.token_urlsafe(16)
    url = "https://accounts.spotify.com/authorize?" + urllib.parse.urlencode(
        {"client_id": cid, "response_type": "code", "redirect_uri": REDIRECT, "scope": SCOPES, "state": state})
    got = {}

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
            if q.get("state", [""])[0] == state:
                got.update({k: v[0] for k, v in q.items()})
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(b"<h2>Done. You can close this tab and go back to the terminal.</h2>")

        def log_message(self, *a):
            pass

    server = http.server.HTTPServer(("127.0.0.1", 8888), Handler)
    print("Opening your browser to approve PixelPressbox...")
    webbrowser.open(url)
    while "code" not in got and "error" not in got:
        server.handle_request()
    if "error" in got:
        raise SystemExit("Spotify said: " + got["error"])

    req = urllib.request.Request(
        "https://accounts.spotify.com/api/token",
        data=urllib.parse.urlencode({"grant_type": "authorization_code", "code": got["code"],
                                     "redirect_uri": REDIRECT}).encode(),
        headers={"Authorization": "Basic " + base64.b64encode(f"{cid}:{secret}".encode()).decode(),
                 "Content-Type": "application/x-www-form-urlencoded"})
    tok = json.load(urllib.request.urlopen(req))
    block = {"client_id": cid, "client_secret": secret, "refresh_token": tok["refresh_token"]}
    print('\nPaste this into config.json:\n\n  "spotify": ' + json.dumps(block) + ",\n")


if __name__ == "__main__":
    main()
