#!/usr/bin/env python3
"""One-time Yahoo Fantasy sign-in. Run on your computer (Python 3, no extra packages):

    python3 yahoo_auth.py

Before running: create an app at https://developer.yahoo.com/apps/create/
  - Redirect URI: https://localhost:8080   (Yahoo's form wants a real-looking URL;
    "oob" also works where the form accepts it)
  - API Permissions: Fantasy Sports -> Read
  - Then confirm API access at https://sports.yahoo.com/developer/application-confirmation/
It prints the "yahoo" block to paste into config.json on the microSD card.
"""
import base64
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request
import webbrowser

AUTH = "https://api.login.yahoo.com/oauth2/request_auth"
TOKEN = "https://api.login.yahoo.com/oauth2/get_token"
API = "https://fantasysports.yahooapis.com/fantasy/v2"
SAVED = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".yahoo_signin.json")  # private, local only


def post_token(cid, secret, body):
    req = urllib.request.Request(TOKEN, data=urllib.parse.urlencode(body).encode(), headers={
        "Authorization": "Basic " + base64.b64encode(f"{cid}:{secret}".encode()).decode(),
        "Content-Type": "application/x-www-form-urlencoded"})
    with urllib.request.urlopen(req) as r:
        return json.load(r)


def get(path, token):
    req = urllib.request.Request(f"{API}/{path}?format=json",
                                 headers={"Authorization": "Bearer " + token})
    with urllib.request.urlopen(req) as r:
        return json.load(r)


def find_teams(node, out):
    if isinstance(node, dict):
        for k, v in node.items():
            if k == "team" and isinstance(v, list):
                flat = {}
                for part in v[0] if v and isinstance(v[0], list) else v:
                    if isinstance(part, dict):
                        flat.update(part)
                if "team_key" in flat:
                    out.append(flat)
            find_teams(v, out)
    elif isinstance(node, list):
        for v in node:
            find_teams(v, out)
    return out


def save(d):
    with open(SAVED, "w") as f:
        json.dump(d, f)
    os.chmod(SAVED, 0o600)


def main():
    if "--resume" in sys.argv and os.path.exists(SAVED):
        # reuse the earlier sign-in (no browser needed)
        d = json.load(open(SAVED))
        cid, secret, redirect = d["cid"], d["secret"], d["redirect"]
        tok = post_token(cid, secret, {"grant_type": "refresh_token", "redirect_uri": redirect,
                                       "refresh_token": d["refresh_token"]})
        if "access_token" not in tok:
            raise SystemExit("Saved sign-in no longer works; run without --resume. Yahoo said: %s" % tok)
        save({**d, "refresh_token": tok.get("refresh_token", d["refresh_token"])})
        return pick_team(cid, secret, redirect, tok)
    cid = input("Yahoo Client ID: ").strip()
    secret = input("Yahoo Client Secret: ").strip()
    redirect = input("Redirect URI you entered on Yahoo's form [https://localhost:8080]: ").strip() \
        or "https://localhost:8080"
    url = AUTH + "?" + urllib.parse.urlencode(
        {"client_id": cid, "redirect_uri": redirect, "response_type": "code", "language": "en-us"})
    print("\nOpening Yahoo sign-in. If no browser opens, visit:\n" + url + "\n")
    webbrowser.open(url)
    if redirect == "oob":
        code = input("Paste the code Yahoo shows you: ").strip()
    else:
        print("After you agree, the browser goes to a page that won't load. That's expected.")
        got = input("Copy the whole address from the address bar and paste it here: ").strip()
        q = urllib.parse.parse_qs(urllib.parse.urlparse(got).query)
        code = q.get("code", [got])[0]
    tok = post_token(cid, secret, {"grant_type": "authorization_code", "redirect_uri": redirect, "code": code})
    if "access_token" not in tok:
        raise SystemExit("Sign-in failed: %s" % tok)
    save({"cid": cid, "secret": secret, "redirect": redirect, "refresh_token": tok["refresh_token"]})
    return pick_team(cid, secret, redirect, tok)


def pick_team(cid, secret, redirect, tok):
    try:
        teams = find_teams(get("users;use_login=1/games;game_codes=nfl,nba,mlb/teams", tok["access_token"]), [])
    except urllib.error.HTTPError as e:
        body = e.read().decode(errors="replace")[:600]
        print("\nSigned in to Yahoo OK, but the Fantasy API refused the request (HTTP %d)." % e.code)
        print("Yahoo said:", body or "(nothing)")
        if e.code in (401, 403):
            print("\nThis usually means Yahoo hasn't turned on Fantasy API access for your app yet.")
            print("Make sure you submitted https://sports.yahoo.com/developer/application-confirmation/")
            print("and that 'Fantasy Sports - Read' is checked on your app at developer.yahoo.com.")
        print("\nYour sign-in is saved, so you don't need to do the browser part again. Later, run:")
        print("    python3 tools/yahoo_auth.py --resume")
        raise SystemExit(1)
    print("\nYour fantasy teams:")
    for i, t in enumerate(teams):
        print(f"  [{i}] {t.get('name')}  ({t['team_key']})")
    choice = 0
    if len(teams) > 1:
        choice = int(input("Which team should the ticker track? number: ") or 0)
    team_key = teams[choice]["team_key"] if teams else "PASTE_TEAM_KEY"

    block = {"client_id": cid, "client_secret": secret, "redirect_uri": redirect,
             "refresh_token": tok["refresh_token"], "team_key": team_key}
    print("\n---- replace the \"yahoo\" block in config.json with this ----")
    print('"yahoo": ' + json.dumps(block, indent=2))
    print("\n(Or paste it at http://pixelpressbox.local and press Save.)")


if __name__ == "__main__":
    main()
