// autoupdate.h - nightly firmware updates from the project's GitHub repo.
// The repo has firmware/manifest.json: {"version": "...", "bin": "https://.../pixelpressbox-update.bin",
// "size": 2600992, "md5": "..."}. If its version differs from ours, download, check the MD5, install, restart.
#pragma once
#include <Arduino.h>

// Check now. Returns a status line for the phone page ("Up to date (2026.11.12-2031)", "Updating to ...", error).
// On a successful install the board restarts and this never returns.
String checkForUpdate();
// Call from the main loop; checks once a day at CFG.autoUpdateHour (local time).
void autoUpdateTick(time_t now);
String lastUpdateStatus();
