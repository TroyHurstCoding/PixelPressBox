// sound.h - speaker via the onboard ES8311 codec + NS4150 amp. Sound files live on the microSD card.
#pragma once
#include <Arduino.h>

bool soundBegin();                 // call after SD card is mounted
void soundLoop();                  // call often (feeds the audio decoder)
bool soundPlay(const String &name, const String &team = "");  // e.g. ("td", "LV")
bool soundBusy();
void soundSetVolume(uint8_t v);    // 0-21

// microphones (ES7210): a spectrum for the visualizer and a double-clap detector, only while asked for
bool micPresent();
void micUse(bool spectrum, bool claps);
int micBands(uint8_t *out, int n);  // 16 bands, low to high, 0-255
bool micTakeDoubleClap();
