// weather.cpp - current conditions + today's high/low from Open-Meteo (free, no API key).
#include "weather.h"
#include "net.h"

bool weatherConfigured() { return CFG.lat != 0 || CFG.lon != 0; }

bool fetchWeather(Weather &w) {
  if (!weatherConfigured()) return false;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(CFG.lat, 4) +
               "&longitude=" + String(CFG.lon, 4) +
               "&current=temperature_2m,apparent_temperature,weather_code,is_day"
               "&daily=temperature_2m_max,temperature_2m_min,uv_index_max,precipitation_probability_max"
               "&forecast_days=1&timezone=auto" +
               String(CFG.fahrenheit ? "&temperature_unit=fahrenheit" : "");
  JsonDocument d;
  if (httpGetJson(url, d) != 200) return false;
  JsonObjectConst cur = d["current"];
  if (cur.isNull()) return false;
  w.temp = (int)lroundf(cur["temperature_2m"] | 0.0f);
  w.code = cur["weather_code"] | 0;
  w.day = (cur["is_day"] | 1) == 1;
  w.hi = (int)lroundf(d["daily"]["temperature_2m_max"][0] | 0.0f);
  w.lo = (int)lroundf(d["daily"]["temperature_2m_min"][0] | 0.0f);
  w.feels = (int)lroundf(cur["apparent_temperature"] | (float)w.temp);
  w.uv = (int)lroundf(d["daily"]["uv_index_max"][0] | 0.0f);
  w.rainPct = d["daily"]["precipitation_probability_max"][0] | 0;
  w.ok = true;
  // air quality is a separate Open-Meteo service; keep the last value if it fails
  JsonDocument a;
  if (httpGetJson("https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + String(CFG.lat, 4) +
                      "&longitude=" + String(CFG.lon, 4) + "&current=us_aqi",
                  a) == 200)
    w.aqi = (int)lroundf(a["current"]["us_aqi"] | -1.0f);
  return true;
}
