// weather.h - Open-Meteo current conditions for the clock face.
#pragma once
#include "model.h"

bool weatherConfigured();
bool fetchWeather(Weather &w);
