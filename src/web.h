// web.h - settings page at http://pixelpressbox.local (edit config, test sounds, restart).
#pragma once
#include <Arduino.h>
#include "model.h"

#include "version.h"  // FW_VERSION, shown on the phone page and in the log

void webBegin();
void webLoop();
// set by the web page, consumed by the main loop
extern volatile bool webWantsRestart;
extern String webTestSound;  // e.g. "td" or "LV_td"
extern volatile bool webNext;       // "Next" button on the phone page
extern volatile bool webToggleFav;  // "Favorites only" button

// desk clock controls from the phone page
extern volatile int webBusyMode;    // -1 = no change, else BusyMode (0 auto, 1 busy, 2 free)
extern volatile int webFocusCmd;    // -1 = no change, 0 = stop, N = start an N-minute focus timer
extern volatile bool webMsLogin;    // "Sign in to Outlook" pressed
extern volatile bool webDeskChanged;// to-do list edited -> rebuild cards
extern String webStatusLine;        // shown at the top of the phone page (set by the main loop)
// pin one card (or "" = rotate through all). Set by the phone page, applied by the main loop.
extern String webPinRequest;
extern volatile bool webPinChanged;
extern volatile bool webCheckUpdate;  // "Check for update" pressed
extern String webCardsJson;         // {"pinned":"...","cards":[{"k":"clock","l":"Clock"},...]} (set by the main loop)
