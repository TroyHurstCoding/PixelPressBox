// settings.h - load/save /config.json on the microSD card.
#pragma once
#include "model.h"

bool sdBegin();
bool loadSettings(const char *path = "/config.json");
String readConfigText(const char *path = "/config.json");
bool writeConfigText(const String &json, const char *path = "/config.json");
