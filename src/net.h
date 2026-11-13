// net.h - HTTPS GET/POST helpers that parse JSON straight from the stream.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// ESPN/Yahoo JSON nests ~15 levels deep; ArduinoJson's default limit (10) is too low.
constexpr uint8_t JSON_NESTING = 40;

// GET url and deserialize into doc using an optional filter. Returns HTTP code (200 = ok).
int httpGetJson(const String &url, JsonDocument &doc, const JsonDocument *filter = nullptr,
                const char *bearer = nullptr);

// POST form body, parse JSON response.
int httpPostFormJson(const String &url, const String &body, const String &authHeader, JsonDocument &doc);

// percent-encode a value for a form body or query string
String urlEncode(const String &s);
