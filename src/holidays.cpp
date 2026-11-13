#include "holidays.h"
#include "model.h"

enum RuleKind { FIXED, NTH, LAST, EASTER, AFTER_NTH, ELECTION };
struct Rule {
  RuleKind kind;
  int month, a, b;  // FIXED: day=a | NTH: n=a, weekday=b | LAST: weekday=b | EASTER: offset=a
                    // AFTER_NTH: nth weekday then +days (n=a, weekday=b, offset in `extra`)
  int extra;
};

// weekday: 0 = Sunday ... 6 = Saturday
static const struct {
  Holiday h;
  Rule r;
} TABLE[] = {
    {{"New Year's Day", "NEW YEAR", "HAPPY", 0xFFD700, true, HI_FIREWORKS}, {FIXED, 1, 1, 0, 0}},
    {{"Martin Luther King Jr. Day", "MLK DAY", "", 0xC080FF, true, HI_STAR}, {NTH, 1, 3, 1, 0}},
    {{"Groundhog Day", "GROUNDHOG", "HAPPY", 0xB08040, false, HI_NONE}, {FIXED, 2, 2, 0, 0}},
    {{"Super Bowl Sunday", "SUPER BOWL", "", 0x30C030, false, HI_STAR}, {NTH, 2, 2, 0, 0}},
    {{"Valentine's Day", "VALENTINES", "HAPPY", 0xFF3070, false, HI_HEART}, {FIXED, 2, 14, 0, 0}},
    {{"Presidents' Day", "PRES. DAY", "HAPPY", 0x4080FF, true, HI_FLAG}, {NTH, 2, 3, 1, 0}},
    {{"Mardi Gras", "MARDI GRAS", "HAPPY", 0xA040FF, false, HI_STAR}, {EASTER, 0, -47, 0, 0}},
    {{"Daylight Saving Time starts", "SPRING FWD", "", 0x60C0FF, false, HI_CLOCK}, {NTH, 3, 2, 0, 0}},
    {{"Pi Day", "PI DAY", "HAPPY", 0x40C0FF, false, HI_NONE}, {FIXED, 3, 14, 0, 0}},
    {{"St. Patrick's Day", "ST PATS", "HAPPY", 0x20D040, false, HI_SHAMROCK}, {FIXED, 3, 17, 0, 0}},
    {{"April Fools' Day", "APRIL FOOL", "", 0xFF8020, false, HI_NONE}, {FIXED, 4, 1, 0, 0}},
    {{"Easter", "EASTER", "HAPPY", 0xFFB0E0, false, HI_EGG}, {EASTER, 0, 0, 0, 0}},
    {{"Earth Day", "EARTH DAY", "HAPPY", 0x30C060, false, HI_LEAF}, {FIXED, 4, 22, 0, 0}},
    {{"Cinco de Mayo", "CINCO MAYO", "", 0x30C040, false, HI_STAR}, {FIXED, 5, 5, 0, 0}},
    {{"Mother's Day", "MOTHER'S", "HAPPY", 0xFF70B0, false, HI_HEART}, {NTH, 5, 2, 0, 0}},
    {{"Memorial Day", "MEMORIAL", "", 0x4080FF, true, HI_FLAG}, {LAST, 5, 0, 1, 0}},
    {{"Flag Day", "FLAG DAY", "HAPPY", 0xFF3030, false, HI_FLAG}, {FIXED, 6, 14, 0, 0}},
    {{"Juneteenth", "JUNETEENTH", "HAPPY", 0xFF4040, true, HI_STAR}, {FIXED, 6, 19, 0, 0}},
    {{"Father's Day", "FATHER'S", "HAPPY", 0x40A0FF, false, HI_STAR}, {NTH, 6, 3, 0, 0}},
    {{"Independence Day", "JULY 4TH", "HAPPY", 0xFF3030, true, HI_FIREWORKS}, {FIXED, 7, 4, 0, 0}},
    {{"Labor Day", "LABOR DAY", "HAPPY", 0x4080FF, true, HI_FLAG}, {NTH, 9, 1, 1, 0}},
    {{"Columbus / Indigenous Peoples' Day", "COLUMBUS", "", 0xC08040, true, HI_LEAF}, {NTH, 10, 2, 1, 0}},
    {{"Halloween", "HALLOWEEN", "HAPPY", 0xFF7A00, false, HI_PUMPKIN}, {FIXED, 10, 31, 0, 0}},
    {{"Daylight Saving Time ends", "FALL BACK", "", 0xFF9030, false, HI_CLOCK}, {NTH, 11, 1, 0, 0}},
    {{"Election Day", "ELECTION", "", 0x4080FF, false, HI_FLAG}, {ELECTION, 11, 0, 0, 0}},
    {{"Veterans Day", "VETERANS", "", 0x4080FF, true, HI_FLAG}, {FIXED, 11, 11, 0, 0}},
    {{"Thanksgiving", "THANKSGIV", "HAPPY", 0xFF9020, true, HI_LEAF}, {NTH, 11, 4, 4, 0}},
    {{"Black Friday", "BLACK FRI", "", 0xB0B0B0, false, HI_NONE}, {AFTER_NTH, 11, 4, 4, 1}},
    {{"Christmas Eve", "XMAS EVE", "", 0x20D040, false, HI_TREE}, {FIXED, 12, 24, 0, 0}},
    {{"Christmas", "CHRISTMAS", "MERRY", 0xFF3030, true, HI_TREE}, {FIXED, 12, 25, 0, 0}},
    {{"Kwanzaa begins", "KWANZAA", "HAPPY", 0x30C040, false, HI_STAR}, {FIXED, 12, 26, 0, 0}},
    {{"New Year's Eve", "NYE", "HAPPY", 0xFFD700, false, HI_FIREWORKS}, {FIXED, 12, 31, 0, 0}},
};
static const int N = sizeof(TABLE) / sizeof(TABLE[0]);

int holidayCount() { return N; }
const Holiday &holidayInfo(int i) { return TABLE[i].h; }

static long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}
static void civilFromDays(long z, int &y, int &m, int &d) {
  z += 719468;
  const long era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = (int)(yoe + era * 400) + (m <= 2);
}
static int weekday(long days) { return (int)(((days % 7) + 11) % 7); }  // 1970-01-01 was a Thursday (4)

void easterDate(int Y, int &month, int &day) {  // anonymous Gregorian algorithm
  int a = Y % 19, b = Y / 100, c = Y % 100, d = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3;
  int h = (19 * a + b - d - g + 15) % 30, i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7;
  int m = (a + 11 * h + 22 * l) / 451;
  month = (h + l - 7 * m + 114) / 31;
  day = ((h + l - 7 * m + 114) % 31) + 1;
}

static long nthWeekday(int y, int m, int n, int wd) {
  long first = daysFromCivil(y, m, 1);
  return first + ((wd - weekday(first) + 7) % 7) + 7L * (n - 1);
}

bool holidayDate(int i, int year, int &month, int &day) {
  const Rule &r = TABLE[i].r;
  long days;
  switch (r.kind) {
    case FIXED: month = r.month; day = r.a; return true;
    case NTH: days = nthWeekday(year, r.month, r.a, r.b); break;
    case AFTER_NTH: days = nthWeekday(year, r.month, r.a, r.b) + r.extra; break;
    case LAST: {
      long last = daysFromCivil(year, r.month + 1, 1) - 1;  // r.month < 12 for every LAST rule
      days = last - ((weekday(last) - r.b + 7) % 7);
      break;
    }
    case EASTER: {
      int em, ed;
      easterDate(year, em, ed);
      days = daysFromCivil(year, em, ed) + r.a;
      break;
    }
    case ELECTION:  // Tuesday after the first Monday in November, even years (federal elections)
      if (year % 2) return false;
      days = nthWeekday(year, 11, 1, 1) + 1;
      break;
    default: return false;
  }
  int y;
  civilFromDays(days, y, month, day);
  return true;
}

bool holidayEnabled(int i) {
  if (CFG.holidays == "off") return false;
  if (CFG.holidays == "federal" && !TABLE[i].h.federal) return false;
  for (auto &s : CFG.holidaySkip)
    if (s.equalsIgnoreCase(TABLE[i].h.name) || s.equalsIgnoreCase(TABLE[i].h.shortName)) return false;
  return true;
}

int holidayOn(int year, int month, int day) {
  for (int i = 0; i < N; i++) {
    int m, d;
    if (holidayEnabled(i) && holidayDate(i, year, m, d) && m == month && d == day) return i;
  }
  return -1;
}

int nextHoliday(int year, int month, int day, int maxDays, int &daysAway) {
  long today = daysFromCivil(year, month, day);
  int best = -1;
  long bestDays = 0;
  for (int i = 0; i < N; i++) {
    if (!holidayEnabled(i)) continue;
    for (int y = year; y <= year + 1; y++) {
      int m, d;
      if (!holidayDate(i, y, m, d)) continue;
      long dd = daysFromCivil(y, m, d) - today;
      if (dd > 0 && dd <= maxDays && (best < 0 || dd < bestDays)) {
        best = i;
        bestDays = dd;
      }
    }
  }
  daysAway = (int)bestDays;
  return best;
}
