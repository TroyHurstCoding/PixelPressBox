// feeds.h - small daily feeds: on this day (Wikipedia), word of the day, dad jokes.
#pragma once
#include "model.h"

// Wikipedia's curated "selected" events for a date. gentle = drop items about deaths/disasters.
bool fetchHistory(int month, int day, bool gentle, std::vector<HistoryEvent> &out);
bool fetchWord(WordOfDay &w);
bool fetchJoke(String &joke);
// true if an "on this day" item is about death or disaster (for the gentle filter)
bool grimHistory(const String &text);
