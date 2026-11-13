// holidays.h - US holidays computed by rule, so the list never goes stale.
// Federal holidays plus popular observances (Mother's Day, Halloween, Easter...).
#pragma once
#include <Arduino.h>

enum HolidayIcon { HI_NONE, HI_HEART, HI_PUMPKIN, HI_TREE, HI_SHAMROCK, HI_FLAG, HI_EGG, HI_LEAF, HI_STAR,
                   HI_FIREWORKS, HI_CLOCK };

struct Holiday {
  const char *name;      // full name (phone page, settings "skip" list)
  const char *shortName; // <= 10 characters for the panel
  const char *greeting;  // "HAPPY", "MERRY", ... ("" = none)
  uint32_t color;
  bool federal;
  HolidayIcon icon;
};

int holidayCount();
const Holiday &holidayInfo(int i);
// month/day of holiday i in a year; false if it doesn't happen that year (e.g. Election Day, odd years)
bool holidayDate(int i, int year, int &month, int &day);
// is holiday i switched on by settings (holidays mode + skip list)?
bool holidayEnabled(int i);

// holiday on a local date, or -1
int holidayOn(int year, int month, int day);
// next enabled holiday strictly after the given local date, within `maxDays`; returns index or -1
int nextHoliday(int year, int month, int day, int maxDays, int &daysAway);

void easterDate(int year, int &month, int &day);
