// sound.h - speaker via the onboard ES8311 codec + NS4150 amp. Sound files live on the microSD card.
#pragma once
#include <Arduino.h>

bool soundBegin();                 // call after SD card is mounted
void soundLoop();                  // call often (feeds the audio decoder)
bool soundPlay(const String &name, const String &team = "");  // e.g. ("td", "LV")
bool soundBusy();
void soundSetVolume(uint8_t v);    // 0-21
