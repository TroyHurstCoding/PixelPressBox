#include "net.h"
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <algorithm>

static void prep(NetworkClientSecure &client) {
  // Verify ESPN / Yahoo certificates against the root CA bundle built into the ESP32 core.
  client.useBuiltinCACertBundle();
}

// Read the whole body into PSRAM first, then parse. Parsing straight off the TLS stream gave up
// ("IncompleteInput") whenever the network paused mid-download on big responses like ESPN's.
static char *readBody(HTTPClient &http, size_t &len) {
  NetworkClient *s = http.getStreamPtr();
  int expected = http.getSize();  // -1 if the server didn't say
  size_t cap = expected > 0 ? expected + 1 : 64 * 1024;
  char *buf = (char *)ps_malloc(cap);
  if (!buf) return nullptr;
  len = 0;
  uint32_t last = millis();
  while (expected < 0 || (int)len < expected) {
    int avail = s->available();
    if (avail > 0) {
      if (len + avail + 1 > cap) {
        size_t ncap = std::max(cap * 2, len + avail + 1);
        char *nb = (char *)ps_realloc(buf, ncap);
        if (!nb) break;
        buf = nb;
        cap = ncap;
      }
      len += s->readBytes(buf + len, avail);
      last = millis();
    } else if (!s->connected() || millis() - last > 15000) {
      break;  // server closed the connection, or nothing for 15 s
    } else {
      delay(2);
    }
  }
  buf[len] = 0;
  if (expected > 0 && (int)len < expected) Serial.printf("[net] short body: %u of %d bytes\n", (unsigned)len, expected);
  return buf;
}

int httpGetJson(const String &url, JsonDocument &doc, const JsonDocument *filter, const char *bearer) {
  NetworkClientSecure client;
  prep(client);
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding
  http.setTimeout(20000);
  http.setUserAgent("PixelPressbox/1.0 (ESP32 desk clock)");  // Wikipedia and icanhazdadjoke ask for one
  if (!http.begin(client, url)) return -1;
  http.addHeader("Accept", "application/json");
  if (bearer) http.addHeader("Authorization", String("Bearer ") + bearer);
  int code = http.GET();
  if (code == 200) {
    size_t len = 0;
    char *body = readBody(http, len);
    if (!body) {
      code = -3;
    } else {
      DeserializationError err = filter
          ? deserializeJson(doc, body, len, DeserializationOption::Filter(*filter),
                            DeserializationOption::NestingLimit(JSON_NESTING))
          : deserializeJson(doc, body, len, DeserializationOption::NestingLimit(JSON_NESTING));
      if (err) {
        Serial.printf("JSON error %s (%u bytes) for %s\n", err.c_str(), (unsigned)len, url.c_str());
        code = -2;
      }
      free(body);
    }
  } else {
    Serial.printf("[net] HTTP %d for %s\n", code, url.c_str());
  }
  http.end();
  return code;
}

int httpPostFormJson(const String &url, const String &body, const String &authHeader, JsonDocument &doc) {
  NetworkClientSecure client;
  prep(client);
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(20000);
  http.setUserAgent("PixelPressbox/1.0 (ESP32 desk clock)");  // Wikipedia and icanhazdadjoke ask for one
  if (!http.begin(client, url)) return -1;
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  if (authHeader.length()) http.addHeader("Authorization", authHeader);
  int code = http.POST(body);
  if (code > 0) {
    size_t len = 0;
    char *resp = readBody(http, len);
    if (!resp) return http.end(), -3;
    DeserializationError err = deserializeJson(doc, resp, len, DeserializationOption::NestingLimit(JSON_NESTING));
    free(resp);
    if (err) code = -2;
  }
  http.end();
  return code;
}

String urlEncode(const String &s) {
  static const char *hex = "0123456789ABCDEF";
  String o;
  o.reserve(s.length() * 3);
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') o += c;
    else {
      o += '%';
      o += hex[((unsigned char)c) >> 4];
      o += hex[((unsigned char)c) & 15];
    }
  }
  return o;
}
