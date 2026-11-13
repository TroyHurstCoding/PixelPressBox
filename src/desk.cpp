#include "desk.h"
#include <algorithm>
#include <cmath>
#include "holidays.h"

// days since 1970-01-01 for a civil date (Howard Hinnant's algorithm)
static long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

time_t parseIsoUtc(const String &s) {
  int y, mo, d, h = 0, mi = 0, se = 0;
  if (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) < 3) return 0;
  if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
  return (time_t)(daysFromCivil(y, mo, d) * 86400L + h * 3600L + mi * 60L + se);
}

String isoUtc(time_t t) {
  struct tm u;
  gmtime_r(&t, &u);
  char b[24];
  strftime(b, sizeof(b), "%Y-%m-%dT%H:%M:%SZ", &u);
  return b;
}

int daysUntil(const Countdown &c, time_t now) {
  struct tm lt;
  localtime_r(&now, &lt);
  long today = daysFromCivil(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
  if (c.year == 0) {  // every year: this year's date, or next year's if it already passed
    long d = daysFromCivil(lt.tm_year + 1900, c.month, c.day) - today;
    return d >= 0 ? (int)d : (int)(daysFromCivil(lt.tm_year + 1901, c.month, c.day) - today);
  }
  return (int)(daysFromCivil(c.year, c.month, c.day) - today);
}

// ---------------------------------------------------------------- sun, moon, progress
static const double DEG = M_PI / 180.0;

static time_t localMidnight(time_t now) {
  struct tm lt;
  localtime_r(&now, &lt);
  lt.tm_hour = lt.tm_min = lt.tm_sec = 0;
  lt.tm_isdst = -1;
  return mktime(&lt);
}

// NOAA "sunrise equation" (Almanac for Computers), good to about a minute
static bool sunEvent(float lat, float lon, int doy, bool rising, double &utHours) {
  double lngHour = lon / 15.0;
  double t = doy + ((rising ? 6.0 : 18.0) - lngHour) / 24.0;
  double M = 0.9856 * t - 3.289;
  double L = fmod(M + 1.916 * sin(M * DEG) + 0.020 * sin(2 * M * DEG) + 282.634 + 720.0, 360.0);
  double RA = fmod(atan(0.91764 * tan(L * DEG)) / DEG + 720.0, 360.0);
  RA += floor(L / 90.0) * 90.0 - floor(RA / 90.0) * 90.0;
  RA /= 15.0;
  double sinDec = 0.39782 * sin(L * DEG), cosDec = cos(asin(sinDec));
  double cosH = (cos(90.833 * DEG) - sinDec * sin(lat * DEG)) / (cosDec * cos(lat * DEG));
  if (cosH > 1 || cosH < -1) return false;
  double H = (rising ? 360.0 - acos(cosH) / DEG : acos(cosH) / DEG) / 15.0;
  double T = H + RA - 0.06571 * t - 6.622;
  utHours = fmod(T - lngHour + 48.0, 24.0);
  return true;
}

bool sunTimes(float lat, float lon, time_t now, time_t &rise, time_t &set) {
  time_t mid = localMidnight(now);
  struct tm lt;
  localtime_r(&now, &lt);
  int doy = lt.tm_yday + 1;
  // midnight UTC of the local calendar date
  time_t utcDay = (time_t)daysFromCivil(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday) * 86400;
  double ur, us;
  if (!sunEvent(lat, lon, doy, true, ur) || !sunEvent(lat, lon, doy, false, us)) return false;
  rise = utcDay + (time_t)(ur * 3600);
  set = utcDay + (time_t)(us * 3600);
  // the UTC hour wraps around midnight; pull both events into the local day
  while (rise < mid) rise += 86400;
  while (rise >= mid + 86400) rise -= 86400;
  while (set < mid) set += 86400;
  while (set >= mid + 86400) set -= 86400;
  return true;
}

float moonPhase(time_t t) {
  const double synodic = 29.530588853, ref = 947182440.0;  // new moon 2000-01-06 18:14 UTC
  double p = fmod(((double)t - ref) / 86400.0 / synodic, 1.0);
  return (float)(p < 0 ? p + 1 : p);
}

const char *moonName(float p) {
  static const char *N[] = {"NEW MOON", "WAX CRESC", "1ST QTR", "WAX GIBB", "FULL MOON",
                            "WAN GIBB", "LAST QTR", "WAN CRESC"};
  return N[(int)floorf(p * 8 + 0.5f) % 8];
}

void dateProgress(time_t now, float &day, float &month, float &year) {
  struct tm lt;
  localtime_r(&now, &lt);
  int y = lt.tm_year + 1900, m = lt.tm_mon + 1;
  double secs = lt.tm_hour * 3600.0 + lt.tm_min * 60.0 + lt.tm_sec;
  day = (float)(secs / 86400.0);
  long d0 = daysFromCivil(y, m, 1), d1 = m == 12 ? daysFromCivil(y + 1, 1, 1) : daysFromCivil(y, m + 1, 1);
  month = (float)(((lt.tm_mday - 1) + secs / 86400.0) / (double)(d1 - d0));
  long y0 = daysFromCivil(y, 1, 1), y1 = daysFromCivil(y + 1, 1, 1);
  year = (float)((lt.tm_yday + secs / 86400.0) / (double)(y1 - y0));
}

String durationText(long secs) {
  if (secs < 0) secs = 0;
  long m = (secs + 59) / 60;  // round up: "in 1M" until it starts
  if (m < 60) return String(m) + "M";
  long h = m / 60;
  m %= 60;
  return m ? String(h) + "H " + String(m) + "M" : String(h) + "H";
}

String clockText(time_t t) {
  struct tm s;
  localtime_r(&t, &s);
  int h = s.tm_hour % 12;
  if (!h) h = 12;
  char b[10];
  if (CFG.twelveHour) snprintf(b, sizeof(b), "%d:%02d%c", h, s.tm_min, s.tm_hour >= 12 ? 'P' : 'A');
  else snprintf(b, sizeof(b), "%d:%02d", s.tm_hour, s.tm_min);
  return b;
}

static bool counts(const Meeting &m) { return !m.allDay && m.showAs != "free" && m.showAs != "workingElsewhere"; }

const Meeting *currentMeeting(const std::vector<Meeting> &ms, time_t now) {
  const Meeting *best = nullptr;
  for (auto &m : ms)
    if (counts(m) && m.start <= now && now < m.end && (!best || m.end < best->end)) best = &m;
  return best;
}

const Meeting *nextMeeting(const std::vector<Meeting> &ms, time_t now) {
  const Meeting *best = nullptr;
  for (auto &m : ms)
    if (counts(m) && m.start > now && (!best || m.start < best->start)) best = &m;
  return best;
}

bool isBusy(const std::vector<Meeting> &ms, time_t now, BusyMode mode) {
  if (mode == BUSY_FORCE) return true;
  if (mode == BUSY_FREE) return false;
  const Meeting *m = currentMeeting(ms, now);
  return m && m->showAs != "tentative";
}

WxKind wxKind(int c) {
  if (c == 0) return WX_CLEAR;
  if (c <= 2) return WX_PARTLY;
  if (c == 3) return WX_CLOUD;
  if (c == 45 || c == 48) return WX_FOG;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return WX_SNOW;
  if (c >= 95) return WX_STORM;
  if (c >= 51) return WX_RAIN;
  return WX_CLOUD;
}

std::vector<Card> buildDeskCards(const std::vector<Meeting> &meetings, const std::vector<String> &todos,
                                 const NowPlaying &np, time_t now, const Weather &wx) {
  std::vector<Card> out;
  struct tm lt;
  localtime_r(&now, &lt);
  int Y = lt.tm_year + 1900, M = lt.tm_mon + 1, D = lt.tm_mday;
  int today = holidayOn(Y, M, D);
  if (today >= 0) {
    const Holiday &h = holidayInfo(today);
    Card c;
    c.kind = CARD_HOLIDAY;
    c.key = String("hol:") + h.shortName;
    c.pri = PRI_HOLIDAY;
    c.dwell = 6;
    c.title = h.greeting;
    c.text = h.shortName;
    c.num = today;
    c.color = h.color;
    out.push_back(c);
  }
  int away = 0;
  int next = nextHoliday(Y, M, D, CFG.holidayCountdownDays, away);
  if (next >= 0) {
    const Holiday &h = holidayInfo(next);
    Card c;
    c.kind = CARD_COUNTDOWN;
    c.key = String("cdh:") + h.shortName;
    c.pri = PRI_COUNTDOWN;
    c.dwell = 5;
    c.title = h.shortName;
    c.num = away;
    c.color = h.color;
    out.push_back(c);
  }
  const Meeting *cur = currentMeeting(meetings, now);
  const Meeting *nxt = nextMeeting(meetings, now);
  if (cur) {
    Card c;
    c.kind = CARD_MEETING;
    c.key = "mtg:now";
    c.pri = PRI_MEETING_SOON;
    c.dwell = CFG.cardDwell;
    c.title = "NOW";
    c.text = cur->subject;
    c.when = cur->start;
    c.until = cur->end;
    c.color = 0xFF2020;
    out.push_back(c);
  }
  if (nxt && nxt->start - now < 12 * 3600) {  // next meeting within 12 hours
    Card c;
    c.kind = CARD_MEETING;
    c.key = "mtg:next";
    bool soon = nxt->start - now <= 15 * 60;
    c.pri = soon ? PRI_MEETING_SOON : PRI_MEETING_LATER;
    c.dwell = CFG.cardDwell;
    c.title = nxt->start - now <= 60 * 60 ? "IN " + durationText(nxt->start - now) : clockText(nxt->start);
    c.text = nxt->subject;
    c.when = nxt->start;
    c.until = nxt->end;
    c.color = soon ? 0xFFC000 : 0x40A0FF;
    out.push_back(c);
  }
  if (!todos.empty()) {
    Card c;
    c.kind = CARD_TODO;
    c.key = "todo";
    c.pri = PRI_TODO;
    c.dwell = CFG.cardDwell;
    c.title = "TO DO " + String((int)todos.size());
    c.items = todos;
    c.color = 0x20FF40;
    out.push_back(c);
  }
  for (size_t i = 0; i < CFG.countdowns.size(); i++) {
    const Countdown &cd = CFG.countdowns[i];
    int d = daysUntil(cd, now);
    if (d < 0) continue;  // already happened
    Card c;
    c.kind = CARD_COUNTDOWN;
    c.key = "cd:" + cd.name;
    c.pri = PRI_COUNTDOWN;
    c.dwell = 5;
    c.title = cd.name;
    c.num = d;
    c.color = cd.color;
    out.push_back(c);
  }
  if (CFG.cardWeather && wx.ok) {
    Card c;
    c.kind = CARD_WEATHER;
    c.key = "wx";
    c.pri = PRI_AMBIENT + 2;
    c.dwell = 6;
    out.push_back(c);
  }
  if (CFG.cardSunMoon && (CFG.lat != 0 || CFG.lon != 0)) {
    Card c;
    c.kind = CARD_SUNMOON;
    c.key = "sunmoon";
    c.pri = PRI_AMBIENT + 1;
    c.dwell = 6;
    out.push_back(c);
  }
  if (CFG.cardProgress) {
    Card c;
    c.kind = CARD_PROGRESS;
    c.key = "progress";
    c.pri = PRI_AMBIENT;
    c.dwell = 5;
    out.push_back(c);
  }
  if (np.playing && np.title.length()) {
    Card c;
    c.kind = CARD_MUSIC;
    c.key = "music";
    c.pri = PRI_MUSIC;
    c.dwell = CFG.cardDwell + 3;
    c.title = np.title;
    c.text = np.artist;
    c.color = 0x1ED760;
    out.push_back(c);
  }
  return out;
}

std::vector<Card> withClock(const std::vector<Card> &cards, int every) {
  Card clock;
  clock.kind = CARD_CLOCK;
  clock.key = "clock";
  clock.dwell = CFG.clockDwell;
  std::vector<Card> out;
  every = std::max(1, every);
  int n = 0;
  for (size_t i = 0; i < cards.size(); i++) {
    if (n % every == 0) {
      Card c = clock;
      c.key = "clock:" + String((int)(n / every));  // unique keys keep the rotator's position stable
      out.push_back(c);
    }
    out.push_back(cards[i]);
    n++;
  }
  if (out.empty()) out.push_back(clock);
  return out;
}

bool ufcCard(const std::vector<FightEvent> &events, time_t now, Card &out) {
  if (!CFG.ufc) return false;
  const FightEvent *best = nullptr;
  for (auto &e : events)
    if (e.end > now && (!best || e.start < best->start)) best = &e;
  if (!best || best->start - now > CFG.ufcDays * 86400L) return false;
  bool live = best->start <= now;
  out = Card();
  out.kind = CARD_UFC;
  out.key = "ufc";
  bool today = live || best->start - now < 18 * 3600;
  out.pri = live ? PRI_FAV_PRE : (today ? PRI_MEETING_LATER : PRI_COUNTDOWN);
  out.dwell = CFG.cardDwell;
  out.text = best->label;
  out.when = best->start;
  out.until = best->end;
  out.color = 0xA61212;
  return true;
}

// ---------------------------------------------------------------- Teams presence
bool presenceBusy(const String &av, const String &act, String &label, uint32_t &color) {
  label = "";
  color = 0x707070;
  if (act == "InACall" || act == "InAConferenceCall") label = "IN CALL";
  else if (act == "InAMeeting") label = "MEETING";
  else if (act == "Presenting") label = "PRESENT";
  else if (av == "DoNotDisturb") label = "DND";
  else if (av == "Busy" || av == "BusyIdle") label = "BUSY";
  else if (av == "Away" || av == "BeRightBack") label = "AWAY";
  bool busy = av == "Busy" || av == "BusyIdle" || av == "DoNotDisturb";
  color = busy ? 0xFF2020 : (label.length() ? 0xFFC000 : 0x707070);
  return busy;
}

// ---------------------------------------------------------------- text helpers
String foldAscii(const String &in) {
  String o;
  const unsigned char *p = (const unsigned char *)in.c_str();
  while (*p) {
    uint32_t cp = *p;
    int n = 1;
    if (cp >= 0xF0) { cp &= 0x07; n = 4; }
    else if (cp >= 0xE0) { cp &= 0x0F; n = 3; }
    else if (cp >= 0xC0) { cp &= 0x1F; n = 2; }
    for (int i = 1; i < n && p[i]; i++) cp = (cp << 6) | (p[i] & 0x3F);
    p += n;
    if (cp < 0x80) { o += (char)cp; continue; }
    if (cp == 0x2018 || cp == 0x2019 || cp == 0x2032) { o += '\''; continue; }
    if (cp == 0x201C || cp == 0x201D) { o += '"'; continue; }
    if (cp == 0x2013 || cp == 0x2014 || cp == 0x2212) { o += '-'; continue; }
    if (cp == 0x2026) { o += "..."; continue; }
    if (cp == 0xA0) { o += ' '; continue; }
    if (cp == 0xB0) { o += (char)0xF8; continue; }  // degree sign in the panel font
    if (cp >= 0xC0 && cp <= 0x17F) {  // Latin-1 + Latin Extended-A letters -> base letter
      static const char *L1 = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";
      if (cp <= 0xFF) { o += L1[cp - 0xC0]; continue; }
      static const char *LA = "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiJjJjKkkLlLlLlLlLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";
      o += LA[cp - 0x100];
      continue;
    }
    // anything else (emoji, CJK...) is dropped
  }
  return o;
}

std::vector<String> wrapText(const String &s, int width) {
  std::vector<String> lines;
  String line;
  int i = 0, n = s.length();
  while (i < n) {
    while (i < n && s[i] == ' ') i++;
    int j = i;
    while (j < n && s[j] != ' ') j++;
    String word = s.substring(i, j);
    i = j;
    if (!word.length()) break;
    while ((int)word.length() > width) {  // very long word: hard break
      if (line.length()) { lines.push_back(line); line = ""; }
      lines.push_back(word.substring(0, width));
      word = word.substring(width);
    }
    if (!line.length()) line = word;
    else if ((int)(line.length() + 1 + word.length()) <= width) line += " " + word;
    else { lines.push_back(line); line = word; }
  }
  if (line.length()) lines.push_back(line);
  return lines;
}

// ---------------------------------------------------------------- standings
bool inSeason(const String &league, time_t now) {
  struct tm t;
  localtime_r(&now, &t);
  int m = t.tm_mon + 1;
  if (league == "nfl") return m >= 9 || m <= 2;
  if (league == "nba") return m >= 10 || m <= 6;
  if (league == "mlb") return m >= 3 && m <= 10;
  return true;
}

static String shortGroup(const String &name) {
  String s = name;
  s.replace("American League", "AL");
  s.replace("National League", "NL");
  s.replace("Western Conference", "WEST");
  s.replace("Eastern Conference", "EAST");
  s.toUpperCase();
  return s;
}

std::vector<Card> standingsCards(const std::vector<StandGroup> &groups, time_t now) {
  std::vector<Card> out;
  if (!CFG.cardStandings) return out;
  for (auto &g : groups) {
    if (!inSeason(g.league, now)) continue;
    int fav = -1;
    for (size_t i = 0; i < g.rows.size(); i++)
      if (CFG.isFavorite(g.league, g.rows[i].abbr)) fav = (int)i;
    if (fav < 0) continue;
    // window of up to 4 rows: the whole division, or the 4 around the favorite in a conference
    int n = (int)g.rows.size(), first = 0;
    if (n > 5) first = std::max(0, std::min(fav - 1, n - 4));
    int last = n > 5 ? first + 4 : n;
    Card c;
    c.kind = CARD_STANDINGS;
    c.key = "st:" + g.league + ":" + g.name;
    c.pri = PRI_AMBIENT + 3;
    c.dwell = 7;
    c.title = shortGroup(g.name);
    for (int i = first; i < last; i++) {
      const StandRow &r = g.rows[i];
      String rec = String(r.wins) + "-" + String(r.losses) + (r.ties ? "-" + String(r.ties) : "");
      // "rank|abbr|record|fav"
      c.items.push_back(String(i + 1) + "|" + r.abbr + "|" + rec + "|" + (i == fav ? "1" : "0"));
    }
    out.push_back(c);
  }
  return out;
}

// ---------------------------------------------------------------- word clock
std::vector<String> wordClockLines(int h, int m) {
  static const char *H[] = {"TWELVE", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX",
                            "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN"};
  int r = ((m + 2) / 5) * 5;  // nearest five minutes
  if (r == 60) { r = 0; h++; }
  int hourNow = h % 12, hourNext = (h + 1) % 12;
  switch (r) {
    case 0: return {H[hourNow], "O'CLOCK"};
    case 5: return {"FIVE", "PAST", H[hourNow]};
    case 10: return {"TEN", "PAST", H[hourNow]};
    case 15: return {"QUARTER", "PAST", H[hourNow]};
    case 20: return {"TWENTY", "PAST", H[hourNow]};
    case 25: return {"TWENTY", "FIVE PAST", H[hourNow]};
    case 30: return {"HALF", "PAST", H[hourNow]};
    case 35: return {"TWENTY", "FIVE TO", H[hourNext]};
    case 40: return {"TWENTY", "TO", H[hourNext]};
    case 45: return {"QUARTER", "TO", H[hourNext]};
    case 50: return {"TEN", "TO", H[hourNext]};
    default: return {"FIVE", "TO", H[hourNext]};
  }
}
