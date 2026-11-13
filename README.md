# PixelPressbox — Seengreat RGB Matrix HUB75 S3 edition (with sound)

Board: **Seengreat RGB Matrix HUB75 S3** (ESP32-S3-WROOM-1-N16R8: 16 MB flash, 8 MB PSRAM).
Pin map and setup come from https://seengreat.com/wiki/214/rgb-matrix-hub75-s3 (see `src/board.h`).

A **desk clock first**: time, date and weather, a busy light and next-meeting countdown from your Outlook
calendar, a meeting chime, to-dos (from the phone page + flagged emails), a focus timer, countdowns and
Spotify now playing. Sports ride along: live MLB / NFL / NBA scores, Dodgers/Raiders/Lakers first, team-color
celebrations **with sound**, a fantasy tracker. Everything is set from a page on your phone.

`screens_preview.png` shows every screen, rendered by the real drawing code.

```
firmware/pixelpressbox-seengreat-s3.bin  first flash over USB (Chrome, address 0x0)
firmware/pixelpressbox-update.bin        later updates over WiFi from the phone page
sdcard/                                   copy onto a microSD card (config.json + sounds/)
src/                                      firmware source (PlatformIO / Arduino)
case/stl/                                 printable case (now with a speaker grille)
tools/                                    yahoo_auth.py, spotify_auth.py, make_images.py, make_sounds.py, release.sh
test/native/                              desktop tests for the logic
test/sim/                                 desktop screen simulator -> screens_preview.png
```

## 1. Flash the board (5 minutes, from Chrome)
1. Plug the board's **USB-C (board power / download)** port into your Mac with a USB-C **data** cable.
2. Open **https://espressif.github.io/esptool-js/** in **Chrome** (not Safari).
3. Click **Connect**, pick the board. If nothing shows up: hold **BOOT**, tap **EN** (reset), release BOOT, then try again.
4. Under Program, choose `firmware/pixelpressbox-seengreat-s3.bin`, Flash Address **0x0**, click **Program**.
5. Tap **RST** when it finishes.

(Developers: `pio run -t upload` from this folder builds and flashes from source.)

### Automatic updates (like Tidbyt)
Every night at 3 AM the clock reads `firmware/manifest.json` from github.com/TroyHurstCoding/PixelPressBox.
If the version there is newer than the one it's running, it downloads `pixelpressbox-update.bin`, checks its
MD5, installs it into the spare app slot and restarts. **Check for update** on the phone page does it right away;
the footer shows the running version and the last check result. `auto_update.enabled` / `auto_update.hour`.
To publish a build: `bash tools/release.sh`, then commit and push `firmware/` and the source.

### Updates over WiFi by hand (after the first USB flash)
1. Open **http://pixelpressbox.local** and tap **Update firmware** (or go to /update).
2. Sign in as **admin** with the password from `update_password` in config.json (default `pixelpressbox`;
   change it).
3. Choose **`firmware/pixelpressbox-update.bin`** (not the big USB file) and press Update. The panel shows
   "UPDATING ... KB", then the clock restarts on the new version. Settings, sounds and to-dos on the SD card stay.
From a phone, put the .bin in iCloud Drive/Files first; from any computer's browser it's one click.
The page footer shows when the running firmware was built. If an update ever won't start, reflash over USB as above.

## Power (from the schematic)
**One cable is enough:** plug a USB-C charger rated 5V/3A into the board's **USB-C** port. A built-in ideal-diode circuit
(Q2 AO4407 + Q3/Q4) feeds the panel from that same port. The separate **POWER** USB-C only adds extra current
for the panel and does not power the board; you don't need it for one 64×32 panel.

## 2. Prepare the microSD card
Copy everything inside `sdcard/` to the root of a FAT32 microSD card:
```
config.json
sounds/td.wav  fg.wav  xp.wav  safety.wav  run.wav  score.wav  startup.wav  win.wav  chime.wav  focus.wav
```
Insert it into the board's TF slot. (Without a card it still runs on built-in defaults, but no sound.)

## 3. First boot: WiFi from your phone
The screen shows **JOIN WIFI**. On your phone, join the WiFi network **PixelPressbox-Setup**, pick your
home WiFi (2.4 GHz) and enter the password. It remembers it from then on.

## 4. Settings from your phone
Open **http://pixelpressbox.local** (same WiFi). You can edit every setting, press **Save & restart**,
and tap buttons to **test each sound** on the speaker.

| Setting | What it does |
|---|---|
| `favorites` | Teams that take over the screen when live (ESPN abbreviations: LAD, LV, LAL) |
| `show_all` | Leagues where every game rotates in (`nfl: true`) |
| `celebrate` | Flash + sound on scores (NBA off by default — it'd never stop) |
| `fantasy_players` | Players whose live stat lines scroll by |
| `night` | Dim hours/brightness. `sound.quiet_at_night` mutes celebrations then |
| `sound.volume` | 0–21 |
| `timezone` | POSIX TZ string; Pacific is `PST8PDT,M3.2.0,M11.1.0` |
| `panel.driver` | `SHIFTREG` (Seengreat default). **Blank/garbled/dim?** try `FM6126A`, then `ICN2038S` |
| `panel.swap_green_blue` | `true` for the Adafruit #5036 panel |

**Pin a card:** on the phone page under **Screen**, pick any card ("Show only: Tetris clock", "LV @ KC",
"Clock"...) or tap **Keep this card on** to freeze whatever's showing. **Rotate all** goes back to cycling.
While pinned, rolling the thumbwheel moves the pin to the next card, so you can browse and stop on one.
The choice survives restarts. If a pinned game ends and drops off, the clock shows until it's back.

**Controls:** the board's **thumbwheel**: roll up/down = next card, **press = start/stop a focus timer**.
BOOT: tap = next, hold 1 s = focus timer. The phone page has Next, Favorites only, Focus 25/50/Stop,
Busy light (Auto / Busy / Free), and the to-do list.

## 4b. Desk clock cards
The clock is the home screen; other cards come and go, and the clock returns after every 2 of them
(`clock.every`, `clock.dwell` seconds).

| Card | What it shows | Setup |
|---|---|---|
| Clock | Time, day/date, weather icon + temperature. Bottom-left turns into **MTG 18M** (yellow, red under 5 min) when a meeting is within an hour | `weather.lat` / `lon` (example is Los Angeles; set yours), `units` F or C |
| Busy light | Red frame around the clock and meeting cards while you're in a busy meeting (tentative doesn't count). Override from the phone | `outlook.busy_light` |
| Meetings | "NOW" with time left, or "IN 18M" / start time for the next one, subject scrolling | Outlook sign-in (below) |
| Meeting chime | Chime + flashing "MEETING IN 2M" | `outlook.chime_minutes` (0 = off) |
| To-do | Your phone-page list plus flagged emails ("MAIL: …") | `outlook.flagged_email` |
| Focus timer | Big countdown, then an automatic break | `focus.minutes`, `focus.break` |
| Countdowns | "VEGAS 38 DAYS" | `countdowns: [{"name":"Vegas","date":"2026-12-20","color":"#FFC000"}]` |
| Now playing | Spotify track, artist, progress bar (only while playing) | Spotify sign-in (below) |
| Sports | Scores and celebrations as before | `sports`: `all`, `favorites` or `off` |

### Outlook (work account)
Microsoft requires an "app registration" for any device that reads your calendar. It takes about 5 minutes:
1. Go to **https://entra.microsoft.com** → App registrations → New registration. Name it PixelPressbox,
   pick "Accounts in any organizational directory", no redirect URI.
2. In the new app: **Authentication** → turn on **Allow public client flows**.
3. **API permissions** → Add → Microsoft Graph → Delegated → `Calendars.Read`, `Mail.ReadBasic`, `Presence.Read`.
4. Copy the **Application (client) ID** into config.json as `outlook.client_id`. Save & restart.
5. On the phone page press **Sign in to Outlook**. The panel and the page show a code; enter it at
   **microsoft.com/devicelogin** and sign in with your work account.

If sign-in says your organization needs **admin approval**, your IT department blocks self-approved apps;
they'd have to approve it (or it won't work on that account). Nothing else breaks without it.

### Holidays
Built in and calculated for any year: New Year's, MLK Day, Groundhog Day, Super Bowl Sunday, Valentine's,
Presidents' Day, Mardi Gras, daylight saving (spring forward / fall back), Pi Day, St. Patrick's, April Fools',
Easter, Earth Day, Cinco de Mayo, Mother's Day, Memorial Day, Flag Day, Juneteenth, Father's Day, July 4th,
Labor Day, Columbus / Indigenous Peoples' Day, Halloween, Election Day (even years), Veterans Day, Thanksgiving,
Black Friday, Christmas Eve, Christmas, Kwanzaa, New Year's Eve.
- On the day: a "HAPPY HALLOWEEN" card with an icon (fireworks on New Year's and July 4th), and the clock's date turns the holiday color.
- Before: a countdown to the next one within `holidays.countdown_days`.
- `holidays.show`: `all`, `federal` or `off`; `holidays.skip`: names to hide.
- Not included yet: holidays on lunar/religious calendars (Lunar New Year, Passover, Ramadan, Eid, Diwali,
  Hanukkah). They need a date table rather than a rule; add them as countdowns meanwhile.
- **Birthdays and anniversaries:** countdowns with `"date": "MM-DD"` repeat every year.

### More cards (inspired by Tidbyt apps)
- **Sun & moon:** sunrise, sunset and the moon phase drawn as a disk (from your weather location, no internet needed).
- **Date progress:** how much of the day, month and year has gone by.
- **Weather details:** high/low, feels-like, chance of rain, UV index and air quality (Open-Meteo, free).
- **Baseball:** teams stacked on the left in their colors; on the right a diamond whose bases fill in when occupied, the inning (arrow up/down = top/bottom), the balls-strikes count and outs.
- **Next UFC event:** date, start time and a countdown, with the event name scrolling across a red bar
  ("LIVE NOW" during the event). From ESPN's UFC schedule; `ufc.show`, `ufc.days_ahead`.
- **Football:** NFL games get a TV-style scoreboard: team-color bars, big scores, who has the ball, timeouts,
  quarter and clock alternating with down & distance ("3&7 KC35"), red corners in the red zone.
Turn the ambient cards on or off under `cards`.

### Batch 2 cards (from the Tidbyt app list)
- **NFL play-by-play:** during live NFL games the last play scrolls across the bottom of the scoreboard
  ("J.Daniels pass deep left ... INTERCEPTED by J.Holland").
- **Standings:** your favorite team's division (NFL: AFC West, MLB: NL West) or, for the NBA, the four teams
  around the Lakers in the West, with your team highlighted. In season only. `cards.standings`.
- **Teams status:** with Outlook signed in, your Teams presence drives the busy light and the clock shows
  "IN CALL", "MEETING", "DND" or "AWAY". `outlook.teams_presence`.
- **Today in history** (Wikipedia), **word of the day**, **dad jokes**: wrapped text that scrolls up.
  `cards.on_this_day_gentle` skips items about deaths and disasters.
- **Tetris clock:** the time builds itself out of falling blocks. **Digital rain:** Matrix-style falling code.
Each can be switched off under `cards`.

### Clock styles
`clock.style` picks the main clock: **classic** (time, date, weather, meeting countdown, busy light),
**big** (full-height beveled segment digits), **nixie** (photographed nixie tubes that slowly breathe),
**words** ("HALF / PAST / ELEVEN", stepping down the screen) or **tetris**. Nixie, big and words are ports
of the Tronbyt community apps Nixel Clock, Big Clock and Fuzzy Clock (Apache-2.0, credits in `NOTICE`). The other styles can also rotate in as their own cards (`cards.nixie_clock`, `big_clock`,
`word_clock`). Only the classic face shows the date, weather and meeting countdown.

### Fun screens and pictures
- **Fireplace** (Tidbyt's Yule Log animation, built into the firmware), **bouncing text** (`cards.bounce_text`, changes color on every wall hit),
  self-playing **Snake** and **Breakout**. No licensed game characters or logos.
- **Digital rain word:** set `cards.rain_word` (e.g. `"TROY"`). Normal rain on a black background, except it never
  falls where the letters are, so the word shows up as black gaps in the rain. Up to 5 letters are big, up to 10 small.
- **Image shuffle:** put pictures in an `images` folder on the microSD card: animated **GIF**, **PNG** or
  **JPG**. They're shown in random order; GIFs loop for the card's time. 64x32 looks best:
  `python3 tools/make_images.py my_pictures out_folder` (needs `pip3 install pillow`) crops and resizes
  photos and keeps GIFs animated (`--fit` adds black bars instead of cropping). Pin "Image shuffle" on the
  phone page for a little photo frame. The clock reads the folder at startup.

### Spotify
Spotify's developer apps now require the app owner to have **Premium**.
1. https://developer.spotify.com/dashboard → Create app, redirect URI `http://127.0.0.1:8888/callback`, Web API.
2. `python3 tools/spotify_auth.py` on your Mac, paste the client ID and secret, approve in the browser.
3. Paste the printed `"spotify"` block into config.json.

## 5. Custom sounds
Put `.mp3` or `.wav` files in `sounds/` on the card. Names the ticker looks for:
`td` (touchdown), `fg` (field goal), `xp` (extra point), `safety`, `run` (baseball), `score`, `win`, `startup`.
**Team-specific** sounds win over defaults: `LV_td.mp3`, `LAD_run.mp3`, `LAL_win.mp3`, etc.
The included ones are original synth sounds made by `tools/make_sounds.py`.

## 6. Yahoo Fantasy (optional)
1. Create an app at developer.yahoo.com/apps/create: redirect URI `https://localhost:8080`,
   permission **Fantasy Sports: Read**. Then confirm API access (name, email, Client ID) at
   sports.yahoo.com/developer/application-confirmation (Yahoo now reviews Fantasy API apps).
2. `python3 tools/yahoo_auth.py` on your Mac → sign in → pick your team.
3. Paste the printed `"yahoo"` block into config.json (or the phone settings page).

## 7. Case (v2 — flush panel, all black)
Generated by `case/build_case.py` (parametric; edit the numbers at the top and rerun). Preview: `case_preview.png`.

**Size:** 164.6 × 84.6 mm front, 46.8 mm deep, plus a 2.4 mm back cover.

**Front:** the panel sits **flush** with the shell's front edge. There's no lip, louver or diffuser. The shell is an open
frame around the panel: a 2 mm wall plus a 0.3 mm gap, so the bezel is about 2.3 mm and every LED, edges included, shows.
A 0.5 mm bevel on the outer front edge hides first-layer squish. **All four sides are plain**: the back cover screws
in from the back (countersunk, flush) into bosses in the shell's inner corners, sloped underneath so the shell still
prints without supports. Cut-away: `case/lid_screws.png`. The 4 screw columns on the back cover are the panel's
stop: the M3×35 screws pull the panel back onto them, so the column length (from `PANEL_D`) decides how flush it
sits. If the panel ends up proud or sunk, measure its thickness, set `PANEL_D`, and reprint just the back cover.
`FLUSH = 0.3` would sink the panel 0.3 mm to protect the edge LEDs from knocks.

**Layout (as seen from the front):**
- The board screws onto 4 standoffs on the back cover, toward the right, 35 mm in from the right wall, with the
  speaker side facing the back. A HUB75 ribbon runs from the board to the panel.
- **Walls:** no openings at all. Power is **hard-wired**: the power supply's own USB-C cable (iUniker
  XSG-0503500HUS, 5.25 V 4 A) plugs into the board and snaps into a slot in the back cover's bottom edge, on the
  right side seen from the front. The slot is a round hole with a slightly narrower neck, so the cable clicks in and
  stays; the back cover's rim has a matching gap. The shell itself needs no cut. `CABLE_D` in `build_case.py` is an
  estimate (4.0 mm): **measure the cable and rerun before printing the back cover.**
- The thumbwheel sits inside the case and isn't used; the accelerometer (tap / double-tap) takes over its jobs.
- **Stand:** 12° lean-back cradle. The front lip is only 2 mm tall so it never covers the flush panel's LEDs; the
  back lip carries the lean and has a slot for the power cable.
- **Back cover:** speaker grille with a gasket ring, 2 mic-hole clusters, and two rows of vents on the side away
  from the board (bottom row lets cool air in, top row lets warm air out). The shell's walls have no slots.
- 4 screw columns on the back cover line up with the panel's corner holes (126.2 × 66.3 mm center to center).
- 32.1 mm of room behind the panel. The board's underside socket ends 11.5 mm from the panel.

**Files** (`case/stl/`):

| Part | Print orientation | Notes |
| --- | --- | --- |
| `fit_test.stl` | Front edge down | The shell's first 10 mm (~30 min). Push the panel in: it should slide in with a hair of play |
| `shell.stl` | Front edge down | No supports. Set **5 wall lines** so the 2 mm wall prints solid |
| `lid.stl` (back cover) | Outside face down | Standoffs point up, no supports |
| `stand.stl` | Flat | 12° desk cradle |

**Print settings:** see [`case/PRINT_SETTINGS.md`](case/PRINT_SETTINGS.md) (Ender-3 S1 Pro, tuned for smooth sides).

**Hardware and threads:** nothing needs tapping or inserts.
- 4× M3×35 button head (back cover → panel): the panel's corner holes are already threaded metal.
- 2× M3×8 (accelerometer → posts in the shell's top wall), self-tapping like the others below.
- 4× **M3×8 countersunk (flat head)** (back cover → the shell's 4 corner bosses): the heads sit flush in the back
  cover, and no screw shows on any side wall, so every side is flat for landscape or portrait. These and the
  4× M2.5×8 (board → standoffs) **self-tap** into undersized printed holes (2.6 mm for M3, 2.1 mm for M2.5). Drive each one in slowly the first
  time and don't overtighten; PLA holds fine for occasional opening. If you'll open it a lot, use M3 heat-set inserts.
- 1 short HUB75 ribbon (comes with the panel).

**Assembly:**
1. Screw the board to the back cover's standoffs, with the speaker facing the grille.
2. Plug the ribbon into the board, then into the panel's input socket; plug in the panel power cable.
3. Set the panel face down on a soft cloth and lower the shell over it.
4. Slide the back cover in (the rim has reliefs for the USB-C and the wheel). Drive the 4 M3×35 screws through the
   back cover into the panel, then the 4 countersunk M3×8 screws through the back cover's corners into the shell.
5. Plug USB-C in from the right side.

**Accelerometer (ADXL345):** hangs from 2 posts on the inside of the shell's top wall, left half (seen from the
front), components and pins facing down, pin row toward the back cover. Sized from `case/GY-291_ADXL345_module.step`:
20.4 × 16.1 mm board, 3.6 mm holes 15.5 mm apart. Two M3×8 screws self-tap into the posts. The posts have 45° ramps
on their front side so the shell still prints without supports. Cross-section: `case/accel_mount.png`.

## What's verified vs. not
- ✅ Firmware **compiles** for the ESP32-S3-WROOM-1-N16R8 with Seengreat's pin map (Arduino core 3.3, 0 warnings).
- ✅ Logic tests pass on desktop: ESPN parsing through the real JSON filter, priorities/takeover,
  score detection + celebration rules, box-score stat lines (MLB slash line, NFL, NBA), calendar time math,
  busy light rules, countdowns (local date, not UTC), desk cards, clock rotation, weather parsing.
- ✅ Every screen rendered on the desktop by the real drawing code (`screens_preview.png`).
  These tests caught a real bug (ESPN's JSON nests deeper than the parser's default limit) — fixed.
- ⚠️ Not yet run on the physical board: panel driver choice, audio, SD card, thumbwheel directions, Yahoo sign-in.
- ⚠️ Not yet tried against the live services: Outlook sign-in + calendar, Spotify. Weather uses the free
  Open-Meteo API (no key).
- ✅ Checked against Seengreat's demo code: HUB75 pins, SD card pins, expander at 0x19, active-low thumbwheel keys,
  mic/amp pins. Panel power needs no switching (schematic).
  If something's off, the serial log (`pio device monitor`, 115200) tells us what.

## Decisions so far
- Power exits the **right side** (port window to be redone for the Seengreat board).
- Speaker + mics are **on the board itself** → grilles next to the board (to be redone for the new layout).
- **All black** prints; the panel sits flush with no louver or diffuser.
- No physical button; the thumbwheel press starts/stops the focus timer, everything else is on the phone page.
- Mostly a desk clock: the clock is the home screen, sports and other cards rotate in between.
- No wall mount for now.

## Board 3D model (case/board_model.py)
`case/stl/seengreat_board.stl` (+ `.glb` with colors), `case/board_drawing.png`, `case/board_spec.json`.
Outline, holes and connector positions are measured from Seengreat's dimensioned photo (about ±0.5 mm).
PCB is a standard 1.6 mm. **Heights marked EST need calipers when the board arrives:**
- [ ] Speaker height above the board (EST 6.0)
- [ ] HUB75 box header height (EST 9.0) and VH-4P power connector height (EST 9.5)
- [ ] Bottom HUB75 socket height (EST 8.5) and **board-to-panel gap when plugged in**
- [ ] Thumbwheel: wheel diameter (EST 13.7), thickness (EST 2.5), how far it sticks out past the edge (EST 6.1)
- [ ] USB-C: how far it sticks out past the edge (EST 1.7) and its height above the board
- [ ] Hole spacing, center to center (EST 60.0 × 52.5)
Edit the numbers at the top of `board_model.py`, re-run it, and the case gets designed from the same file.

## To-do / later
- [ ] **Measure the panel thickness** when it arrives → set `PANEL_D` in `case/build_case.py`, re-run.
- [ ] **Test-fit print** (`fit_test.stl`) before the full shell print.
- [ ] Team logos (16×16 pixel art from the SD card).
- [ ] Voice commands (ESP-SR on the board's two mics): "next", "start focus", "what's next".
- [ ] Keyhole slots for wall mounting (if it ever goes on a wall).
- [ ] Two-tone / team-color front frame after the first test run.
