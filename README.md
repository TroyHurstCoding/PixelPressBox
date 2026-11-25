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

**More than one WiFi:** the clock remembers every network it has joined (up to 8) and, at power-up or when the
signal drops, joins whichever one is in range, strongest first. Add another ahead of time on the phone page under
**System > WiFi** (name + password), or just take the clock there: when no remembered network is in range it opens
**PixelPressbox-Setup** again and the new one is added to the list. **Forget** removes one. The list is kept in the
board's own memory, not on the SD card.

## 4. Settings from your phone
Open **http://pixelpressbox.local** (same WiFi). The page has four tabs along the bottom:
- **Remote:** pick or pin the screen, Next, favorites only, focus timer, busy light, games, test each sound.
- **To-do:** add, tick off and clear to-dos.
- **Settings:** the everyday options as dropdowns, switches and sliders (clock face, time zone, brightness, night
  dimming, teams, which screens rotate, countdowns, sound, motion sensor...). Change what you like, then
  **Save & restart**. Settings the page doesn't show are left exactly as they were.
- **System:** WiFi networks, Outlook sign-in, firmware updates, restart, and **Edit the settings file** for the
  rarely changed options in the table below (the raw config.json).
To try the page on a computer without the clock: `python3 tools/preview_page.py`, then open http://localhost:8377.

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
| `panel.flip` | `true` if the picture is upside down when the clock stands the way you want it (the LED panel is installed the other way up) |

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
1. Open [Microsoft Entra](https://entra.microsoft.com/) with your work or school account. Under **App registrations → New registration**, name the app **PixelPressbox**. Choose **Accounts in any organizational directory** to use the clock's default tenant setting. Leave the redirect URI blank.
2. In the app's **Overview**, copy the **Application (client) ID**. Under **Authentication → Advanced settings**, set **Allow public client flows** to **Yes** and save. The clock uses a device code, so setup needs no client secret. [Microsoft public client setup](https://learn.microsoft.com/en-us/entra/identity-platform/scenario-desktop-app-registration)
3. Under **API permissions → Add a permission → Microsoft Graph → Delegated permissions**, add `Calendars.Read`, `Mail.ReadBasic`, and `Presence.Read`. The firmware requests all three during sign-in. If your organization requires admin consent, ask IT to approve the app and device-code sign-in. [Microsoft Graph permissions](https://learn.microsoft.com/en-us/graph/permissions-reference)
4. On the clock's local page, open **System → Edit the settings file**. In the existing `outlook` object, set `"client_id"` to the ID you copied. If IT registered the app as single-tenant, set `"tenant"` to the **Directory (tenant) ID** from Overview. Save and restart.
5. Under **System**, choose **Sign in to Outlook**. Enter the clock's code at [Microsoft device sign-in](https://microsoft.com/devicelogin) and use the account whose primary calendar should appear on the clock. Verify with an upcoming timed event in that calendar.

### Sync Microsoft To Do

Add **Microsoft Graph → Delegated permissions → Tasks.Read** to the same Entra app. On PixelPressbox, enable **Sync Microsoft To Do** under Settings and save and restart. If the clock was already signed in, sign out and sign back in under **System** to grant the new permission.

Open tasks appear in the existing To-do list on the panel and the phone page. The clock refreshes them on the Outlook polling interval (three minutes by default) and displays up to 30 open tasks from the first eight lists returned by Microsoft. Complete or edit synced tasks in Microsoft To Do. The clock keeps its own editable tasks on the microSD card.

### Use Apple Calendar with PixelPressbox

On your iPhone, open **Settings → Apps → Calendar → Calendar Accounts → Add Account → Microsoft Exchange** and add the same account linked to PixelPressbox. In Apple Calendar, choose that account's primary calendar when creating an event. You can also choose it under **Settings → Apps → Calendar → Default Calendar**. For eligible existing events, edit the event and change its calendar. Events stored only in iCloud require a separate integration. [Apple Calendar account and default-calendar setup](https://support.apple.com/en-euro/guide/iphone/iphc37be2016/ios)

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

### Batch 3 cards
Turn any of them off under `"cards"` in config.json (keys in brackets).
- **Aquarium** (`aquarium`): fish, swaying seaweed, rising bubbles. No internet needed.
- **Double pendulum** (`double_pendulum`): chaotic physics with a fading color trail; never repeats.
- **Newton's cradle** (`newtons_cradle`): five swinging balls. Without the accelerometer it plays by itself (1, 2, 3
  balls in turn). With it, tilting the clock tilts the strings' "down", and a **tap** knocks the end ball.
- **Water** (`water`): a tank of water that pours toward whichever way is down. Tilt the clock and it sloshes;
  without the accelerometer it rocks gently by itself. Works in landscape and portrait.
- **Trivia** (`trivia`): a question, a few seconds to think, then the answer. Questions from the Open Trivia Database
  (opentdb.com, CC BY-SA 4.0).
- **Fun fact / quote** (`facts_quotes`): alternates a random fact (uselessfacts.jsph.pl) and a quote ("Inspirational
  quotes provided by ZenQuotes API", zenquotes.io).
- **Billboard throwback** (`billboard`): the #1 (and #2) song this week 20, 30 and 40 years ago (`billboard_years`).
  Chart data from github.com/mhollingshead/billboard-hot-100; Billboard and Hot 100 are Billboard Media's trademarks.
- **Fly the W** (`fly_the_w`): after your MLB favorite wins (Dodgers), a waving white W flag with the score, for
  about a day. Losses don't get a card.
- **Space station map** (`iss_map`): a world map with the night side shaded and a blinking dot where the ISS is
  right now (wheretheiss.at, every minute); a red dot marks home.
- **Planets tonight** (`planets`): which of Mercury, Venus, Mars, Jupiter and Saturn are up, where to look and how
  high, calculated on the clock from your weather location. In daytime it shows 9 PM tonight.
- **National day** (`national_day`): "World Kindness Day" and friends (github.com/jvivona/tidbyt-data).
- **Surf report** (`surf`): wave height range, period, direction and water temperature for a beach
  (`"surf": {"name": "Venice", "lat": 33.985, "lon": -118.475}`), from Open-Meteo's free marine forecast.
- **Hacker News** (`hacker_news`): the top 3 headlines.

### Batch 4 cards
Turn any of them off under `"cards"` in config.json, or with the chips under Settings > Screens in the rotation.
- **Binary clock** (`binary_clock`): one column of dots per digit (8-4-2-1), hours red, minutes green, seconds blue.
- **Sun-arc clock** (`sun_clock`): the sun (or a crescent moon at night) on its real path across the sky for your
  weather location, sky colours from the sun's height, sunrise and sunset in the corners.
- **City clock** (`city_clock`): a skyline whose windows light up through the evening, with traffic and stars.
- **World clock** (`world_clock`): up to 4 cities with a day/night dot; a city already on tomorrow's date is tinted
  blue, one still on yesterday's orange. `"world_clocks": [{"name": "LON", "utc": 0, "dst": "eu"}]`: `utc` is the
  standard-time offset in hours, `dst` is `us`, `eu`, `au` or `""`.
- **Sand clock** (`sand_clock`): the minutes pile up as sand (24 grains a minute); at the top of the hour the floor
  opens and it drains. Tilt the clock and the sand slides.
- **Win probability** (`win_probability`): ESPN's win-probability line for your favorite teams' games, live and for
  a while after the final. While live it joins the takeover rotation with the scoreboard.
- **Top performers** (`top_performers`): after your favorite's game, each team's best line ("J. ALLEN 304 YDS 1 TD").
- **Next game** (`next_game`): your favorite team's next game (ESPN team schedule): opponent, day and time, a
  countdown, TV, and the betting line and over/under once ESPN posts them. It replaces the plain pre-game card.
- **Golf leaderboard** (`golf`): the top 10 of the current PGA Tour event while it's on and for a few days after
  (`"golf": {"tour": "pga"}`; ESPN also has `lpga`, `eur`, `liv`).
- **Fantasy points** (`fantasy_points`): see Fantasy football below.
- **Game of Life** (`game_of_life`): Conway's Life on a wrap-around board; newborn cells bright, old ones violet. It
  starts over when it settles. A tap drops in a glider.
- **Sound visualizer** (`visualizer`): a 16-band spectrum from the board's own microphones. Only in the rotation
  when the microphones answer at startup.
- **Planes overhead** (`planes`): a radar with the planes within `"planes": {"radius_km": 40}` of your weather
  location, and the nearest ones' callsign, altitude, speed, heading and distance. From the OpenSky Network
  (anonymous, every 5 minutes to stay inside its daily limit).

### Fantasy football (points, your league's scoring)
Settings > **Fantasy football** on the phone page:
- **Search** a player by name (the phone asks ESPN directly) and add him to your team or the opponent's; add a team
  defence from the list.
- Or **paste a whole roster**: one player per line, `[me]` and `[opp]` sections; OctoBoxVR's `roster.txt` works as
  is. Each line is matched on ESPN and reported `ok`, `MISS` or `AMBIG`; add `, POS, TEAM` to a line that misses.
- **Scoring rules** are prefilled with the OctoBoxVR league (half PPR, 5-point passing TDs, 0.04 per passing yard,
  -2 per interception and lost fumble, kicker 3/1 with -2/-1 for misses, 2-point bonuses at 400 passing / 150 rushing /
  150 receiving yards, the D/ST points-allowed ladder). Any bonus named `passNNN`, `rushNNN` or `recNNN` works.
- **Week**: this week (ESPN's current week) or a chosen one.
The card shows the matchup total, then each side's players: green = on the field right now (for a defence: when the
other team has the ball), white = playing, grey = done, dark = not started, dim red = no game this week. Scores come
from ESPN's box scores, the same way OctoBoxVR's `fantasy-poll.mjs` does it, and a finished game is downloaded once.
Limits: field goals are a flat amount (no kick distances in the box score) and 2-point conversions aren't counted.
The older "Stat lines to follow" list (`fantasy_players`) still shows box-score lines for any league.

### Batch 5 cards
Preview: `screens_preview_batch5.png`. Every card can be switched off under Settings, Screens in the rotation.
- **Clocks:** split-flap board (digits flip with a click, second row = your team's next game), analog clock with
  three dials (temperature, date, a ring that fills as the next game gets closer), terminal (`$ date`, a `| / - \`
  spinner, the odd fake command), slot machine (reels spin each minute, jackpot on the hour), and **life in weeks**
  (set your birthday under Settings; one dot per month of an 80-year life, this month blinking).
- **Fun:** robot eyes (follow the tilt, light up when your team scores, sleepy at night), fortune ball (shake or tap
  for an answer), party tools (shake to roll dice / flip a coin / spin a wheel; tap to switch), maze (carved and solved
  on screen), and the **pixel pet**: tap to feed it, tilt to roll its ball; it sleeps at night, cheers when your team
  scores, grows from an egg over a week of care, and gets grumpy if ignored. It never dies.
- **Sand timer:** double tap, choose TIMER, then tap once per 15 seconds; it starts 3 s after the last tap. While it
  runs, tap = +15 s, double tap = stop. Or use Sand timer on the phone's Remote tab.
- **Sports:** last play (live favorite games: score bug, down and distance, the play), playoff race (seed, games back,
  magic number), and **pick'em**: tap your winners under Settings, Pick'em; the card shows wins and losses as games end
  and keeps your season record.
- **News alerts:** ESPN headlines about your teams and fantasy players drop down as a banner over whatever is showing
  (with a chime), and the last ten are on the "News alerts" card. Choose leagues, keywords (trade, injury, out...)
  and quiet hours under Settings, News alerts. The first download after a restart fills the list without banners.
- **Data:** next rocket launch (Launch Library 2; takes over 10 minutes before liftoff, flags Vandenberg launches),
  ride waits (queue-times.com; Disneyland by default), stocks (Robinhood stock tokens: pick symbols under Settings,
  Stocks; every 2 minutes on weekdays; day change is measured from the last price the clock saw the previous day, so
  it shows after the clock has been on through one close), drive time (needs a free TomTom key and a destination
  under Settings, Drive time; a priority card in your weekday commute hours), Powerball (last numbers and a countdown
  to the next draw; no free source has the jackpot amount, so it isn't shown).
- **BLOCKFALL** (game): stand the clock on its end and shake it. Tilt to move, tap to rotate, lean back to drop
  (a quick hard lean drops it all the way).

### Microphones
The board's ES7210 microphones feed the sound visualizer and, if you turn it on, **clap control**: two claps in a
row = next screen (`"mic": {"clap_control": true, "sensitivity": "high"}`). Claps are ignored while the clock is
playing a sound.

### Accelerometer and portrait mode
The ADXL345 (GY-291 board) plugs into the board's 4-pin **I2C expansion port** (the small SH1.0 socket) with a
STEMMA QT / Qwiic to female-socket cable (Adafruit 4397): VCC to VCC, GND to GND, SDA to SDA, SCL to SCL. On the GY-291
also join **CS to VCC** (I2C mode); leave SDO unconnected (address 0x53). The clock finds it at startup; without it
everything works as before.
- **Tap** the case: next card. **Double tap:** opens a small menu on the panel: **FOCUS** (start the timer), **GAMES**
  (the games menu), **FAVS ONLY** (toggle), **CLOCK** (choose the face). Tap moves down the list, double tap chooses, 6 seconds of nothing
  closes it. How hard a tap has to be is a dropdown in Settings > Motion sensor (`"motion": {"tap_force": ...}`:
  `verylight`, `light` (default), `firm`). On the cradle and the beer glass a single tap plays with
  the screen instead and a double tap moves on.
- **Choose a clock face on the device:** stand it in landscape, double tap on an ordinary clock screen, single-tap to **CLOCK**, then double tap to open it. Single-tap through the faces and double tap or press the wheel to save. **P: DIGITS** and **P: ANALOG** control the portrait face. A writable microSD card makes the choice persist after restart.
- **Turning it:** the picture stays as it is until the clock has been turned more than halfway (45°, plus a few
  degrees so it can't flicker) toward the next way up.
- **Calibrate** (phone page, System > Motion sensor): press one button with the clock standing upright and one with
  it on its back. That measures how the sensor is mounted, whichever way that is. The same card shows the live
  readings; the USB log prints them once a second too.
- **Lay it flat** (on its face or back) for 2 seconds: the screen turns off; pick it up and it comes back. This only
  happens after calibrating, so a sensor mounted some other way round can never blank a standing clock.
- **Upside down:** everything flips to read upright.
- **Portrait:** stand it on either end and it switches to the tall screens, with the tall clock in between:
  - **Beer glass** (`beer`): pours itself a pint, lets the head settle, then sips. Tilt it and the beer stays level;
    tap it for a refill.
  - **Tall clock** (`portrait_clock`): hours over minutes in big digits.
  - **Hourglass** (`hourglass`): sand runs through the neck. Turn the clock over and it runs the other way; without
    the accelerometer it turns itself over when it runs out.
  - **Lava lamp** (`lava_lamp`): blobs heat at the bottom, rise, cool and sink.
  - **Water** works standing up too.
- Settings: `"motion": {"mount": "panel", "orientation": "auto", "mirror": false, "taps": true, "flat_sleep": true, "pitch_invert": false}`.
  `mount` says where the little board sits: `"panel"` = flat on the back of the LED panel on one of its screws, pin
  row along the lower edge, pins pointing at the back cover (the default); `"fin"` = the earlier position, hanging
  chip-down under a fin on the back cover. Which way up it reads is worked out at power-up, standing in landscape.
  `orientation` can force `"landscape"` or `"portrait"` (portrait screens even without the accelerometer).
  If portrait shows upside down, set `"mirror": true` (it depends on which way the little board was mounted).

### Games
**Shake** the clock (a few firm shakes) or use **Games** on the phone page to open the games menu. Tilt left/right
to pick a game, tap to start. In a game, **double tap** (or **Stop** on the phone) goes back to the cards. After a
game over, tap to play again; otherwise it returns to the menu. Best scores are kept on the clock.
- **Dodger:** fly a plane low through a forest. Tilt to bank around the trees.
- **TiltyBird:** lean the clock back to climb, forward to dive, through the gaps in the pipes.
- **MarbleRun:** tilt a marble through a maze to the exit before time runs out; each maze adds time.
- **Catch:** tilt to move the basket. Catch apples and gold, dodge bombs. Three lives.
- **Breakout:** tilt to move the paddle and clear the bricks.
- **Racer:** steer down a winding road around the traffic.
- **Balance:** keep a ball on a see-saw while wind gusts push it.
- **Pong:** tilt your paddle (bottom) against the clock's (top). Three lives; score a point each time it misses.
- **Simon:** watch the colours, then repeat them: tilt to choose a column, tap to press it.
- **Lander:** lean back to fire the engine, tilt to drift, and touch down gently on the yellow pad. Each landing
  scores 50 plus leftover fuel, and the next pad is smaller with stronger gravity.
- **BLOCKFALL** (portrait only): stand the clock on its end and shake it, or press it on the phone. Tilt to move,
  tap to rotate, lean back to drop faster, a quick hard lean to drop all the way.
- **Reaction:** wait for green, then tap. Five rounds; the score is 1000 minus your average time in milliseconds
  (timed to the frame, about 33 ms).
"Lean back" is measured from how you're holding the clock when the game starts. If it comes out backwards, set
`"motion": {"pitch_invert": true}`. Without the accelerometer the games play themselves as a demo (a minute each).
Desktop check: `test/sim/games_sim.cpp` plays every game on its autopilot. `bash tools/desktop_build.sh` builds and
runs all the desktop tests and screen sims (after one `pio run`, which downloads the libraries it needs).

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

## 7. Case (v3 — flush panel, snap-together, all black)
Generated by `case/build_case.py` (parametric; edit the numbers at the top and rerun). Preview: `case_preview.png`.

**Size:** 164.6 × 84.6 mm front, 46.8 mm deep, plus a 2.4 mm back cover.

**How it goes together:** the shell is an empty sleeve. Everything else lives on the back cover as one module: the
panel (screwed to the back cover's 4 columns), the board, and the power cable. The module slides into the shell from
the back and **snaps in, with no screws**. The shell has nothing inside it at all: 4 small bumps on the back cover's rim click into shallow pockets on the
inside of the shell's top and bottom walls, a third of the way in from each end where the walls flex most. Each
bump's catch face is a 45° slope that bears on the pocket's edge, so it pulls the back cover tight against the
shell. 8 tiny crush ribs on the rim stop it rattling. **To open:** spread the top and bottom walls near the back edge
(or slip a thumbnail or guitar pick into the seam) and lift the back cover out; everything comes with it.
Cut-away: `case/snap_fit.png`.

**Front:** the panel sits **0.3 mm behind** the shell's front edge (`FLUSH`; no lip, louver or diffuser), so
print tolerance can't leave the LEDs proud. The bezel is a 2 mm
wall plus a 0.3 mm gap, so every LED, edges included, shows. A 0.5 mm bevel softens the outer front edge. The back
cover landing on the shell's back edge sets the depth, and the columns' length (from `PANEL_D`, the panel's
thickness) sets that recess: if it ends up proud or sunk too far, measure the panel, set `PANEL_D` and reprint the back
cover. **All four sides are plain**: no screws, no openings, for landscape or portrait.

**Layout (as seen from the front):**
- The board screws onto 4 standoffs on the back cover, 35 mm in from the right wall, speaker side facing the back.
  A HUB75 ribbon runs from the board to the panel.
- **Power is hard-wired**: the power supply's own USB-C cable (iUniker XSG-0503500HUS, 5.25 V 4 A) plugs into the
  board and snaps into a slot in the back cover's bottom right corner (seen from the front), beside the stand. `CABLE_D` in
  `build_case.py` is an estimate (4.0 mm): **measure the cable and rerun before printing the back cover.**
- The thumbwheel sits inside the case and isn't used; the accelerometer (tap / double-tap) takes over its jobs.
- **Back cover:** speaker grille, plus a row of 4 vent slots beside it (level with the grille, under the
  accelerometer). No mic holes: the firmware doesn't use the microphones. At desk-clock brightness the panel and
  board run cool; keep the brightness moderate for screens that stay bright white for hours.
- **Stand:** 12° lean-back cradle. The front lip is 2 mm tall so it never covers the LEDs; the power cable leaves the
  back cover beside the cradle's end.
- 32.1 mm of room behind the panel. The board's underside socket ends 11.5 mm from the panel.

**Files** (`case/stl/`):

| Part | Print orientation | Notes |
| --- | --- | --- |
| `fit_test.stl` | Front edge down | The shell's first 10 mm (~30 min). Push the panel in: it should slide in with a hair of play |
| `shell.stl` | Front edge down | No supports. Set **5 wall lines** so the 2 mm wall prints solid. **PETG** if you'll open it often (PLA snaps fine dozens of times) |
| `lid.stl` (back cover) | Outside face down | Columns and standoffs point up, no supports |
| `stand.stl` | Flat | 12° desk cradle |

**Print settings:** see [`case/PRINT_SETTINGS.md`](case/PRINT_SETTINGS.md) (Ender-3 S1 Pro, tuned for smooth sides).

**Hardware:** nothing needs tapping or inserts.
- 4× M3×35 button head (back cover's columns → panel's threaded corner holes, 4.5 mm in). Heads sit in recesses.
- 4× M2.5×8 (board → standoffs): these self-tap into
  undersized printed holes. Drive each one in slowly the first time and don't overtighten.
- 1 short HUB75 ribbon (comes with the panel).

**Assembly** (on the bench, panel face down on a soft cloth):
1. Screw the board to the back cover's standoffs, speaker facing the grille.
2. Plug the ribbon into the board and the panel's input socket; plug in the panel power cable.
3. Set the back cover on the panel (columns on the panel's corner holes) and drive the 4 M3×35 screws.
4. Plug the power cable into the board and snap it into the slot at the back cover's bottom edge.
5. Put the accelerometer on the panel's top-middle screw, flat against the panel, pin row along the lower edge,
   and plug its 4 jumper wires into the board (do this before step 3 if the wires are short).
6. Slide the module into the shell from the back until the 4 catches click.

**Accelerometer (ADXL345):** screwed flat onto the back of the LED panel with the panel's own top-middle screw,
pin row along the lower edge, pins pointing at the back cover (`"motion": {"mount": "panel"}`). The back cover
carries nothing for it. Check that the pins and jumper plugs clear the Seengreat board behind them before closing
the case; right-angle leads help.

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
- **All black** prints; the panel sits 0.3 mm behind the bezel with no louver or diffuser.
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
