// yahoo.h - Yahoo Fantasy live matchup via the official API (OAuth 2.0 refresh token).
#pragma once
#include "playlist.h"

bool yahooConfigured();
// Fills a CARD_YAHOO card. Returns false on failure.
bool yahooMatchup(time_t now, Card &out);
