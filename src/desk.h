// desk.h - desk-clock logic with no hardware or network: calendar math, countdowns, which cards to show.
#pragma once
#include "playlist.h"

// "2026-11-12T17:00:00.0000000" (Graph, UTC) -> epoch seconds. 0 if unparseable.
time_t parseIsoUtc(const String &s);
// epoch -> "2026-11-12T17:00:00Z"
String isoUtc(time_t t);
// whole local days from today to the countdown date (0 = today, negative = past)
int daysUntil(const Countdown &c, time_t now);
// "12M", "1H 5M", "3H"
String durationText(long secs);
// "3:30P" (local)
String clockText(time_t t);

const Meeting *currentMeeting(const std::vector<Meeting> &m, time_t now);  // busy/tentative/oof only
const Meeting *nextMeeting(const std::vector<Meeting> &m, time_t now);     // next timed, not-free event
enum BusyMode { BUSY_AUTO = 0, BUSY_FORCE = 1, BUSY_FREE = 2 };
bool isBusy(const std::vector<Meeting> &m, time_t now, BusyMode mode);
// Teams presence -> busy light. Returns true if busy; label gets "IN CALL"/"MEETING"/"DND"/"AWAY"/"" and a color.
bool presenceBusy(const String &availability, const String &activity, String &label, uint32_t &color);

// word clock: "HALF PAST ELEVEN" as lines, nearest five minutes ("QUARTER" / "TO" / "TWO")
std::vector<String> wordClockLines(int hour24, int minute);

// text helpers
String foldAscii(const String &s);                       // curly quotes, dashes, accents -> plain ASCII
std::vector<String> wrapText(const String &s, int width);  // word wrap to lines of <= width characters

// standings: pick the tables to show (the group with a favorite team; NBA: a 4-team window around it)
bool inSeason(const String &league, time_t now);
std::vector<Card> standingsCards(const std::vector<StandGroup> &groups, time_t now);

// Weather icon groups for a WMO code
enum WxKind { WX_CLEAR, WX_PARTLY, WX_CLOUD, WX_FOG, WX_RAIN, WX_SNOW, WX_STORM };
WxKind wxKind(int wmo);

// sunrise/sunset for the local day containing `now` (epoch seconds); false at polar day/night
bool sunTimes(float lat, float lon, time_t now, time_t &rise, time_t &set);
// moon phase 0..1 (0 = new, 0.5 = full) and a short name like "WAX GIBB"
float moonPhase(time_t t);
const char *moonName(float phase);
// fraction 0..1 of the current local day, month and year that has gone by
void dateProgress(time_t now, float &day, float &month, float &year);

// cards for meetings, to-dos, countdowns, holidays, music and the ambient cards
std::vector<Card> buildDeskCards(const std::vector<Meeting> &meetings, const std::vector<String> &todos,
                                 const NowPlaying &np, time_t now, const Weather &wx = Weather());
// the next UFC event (or the live one) as a card, if it's within CFG.ufcDays
bool ufcCard(const std::vector<FightEvent> &events, time_t now, Card &out);
// [clock, c1..cN, clock, cN+1...]: the clock returns after every `every` other cards
std::vector<Card> withClock(const std::vector<Card> &cards, int every);
