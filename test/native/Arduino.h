// Minimal Arduino shim so the ticker logic can be unit-tested on a desktop compiler.
#pragma once
#include <strings.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <string>

class String {
 public:
  std::string s;
  String() {}
  String(const char *c) : s(c ? c : "") {}
  String(const std::string &x) : s(x) {}
  String(char c) : s(1, c) {}
  String(int v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}
  String(unsigned v) : s(std::to_string(v)) {}
  String(unsigned long v) : s(std::to_string(v)) {}
  String(float v, int dec = 2) { char b[32]; snprintf(b, 32, "%.*f", dec, v); s = b; }
  String(double v, int dec = 2) { char b[32]; snprintf(b, 32, "%.*f", dec, v); s = b; }
  const char *c_str() const { return s.c_str(); }
  unsigned length() const { return s.size(); }
  bool concat(const char *c) { s += c; return true; }
  bool concat(const char *c, size_t n) { s.append(c, n); return true; }
  bool concat(char c) { s += c; return true; }
  void reserve(size_t n) { s.reserve(n); }
  String substring(int a, int b = -1) const {
    if (a < 0) a = 0;
    if (a > (int)s.size()) return String();
    if (b < 0 || b > (int)s.size()) b = s.size();
    if (b < a) return String();
    return String(s.substr(a, b - a));
  }
  int indexOf(char c) const { auto p = s.find(c); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(const String &x) const { auto p = s.find(x.s); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(char c, int from) const { auto p = s.find(c, from); return p == std::string::npos ? -1 : (int)p; }
  int lastIndexOf(char c) const { auto p = s.rfind(c); return p == std::string::npos ? -1 : (int)p; }
  bool startsWith(const String &x) const { return s.rfind(x.s, 0) == 0; }
  bool endsWith(const String &x) const {
    return s.size() >= x.s.size() && s.compare(s.size() - x.s.size(), x.s.size(), x.s) == 0;
  }
  void toLowerCase() { for (auto &c : s) c = tolower(c); }
  void toUpperCase() { for (auto &c : s) c = toupper(c); }
  void trim() { size_t a = s.find_first_not_of(" \t\r\n"); size_t b = s.find_last_not_of(" \t\r\n"); s = a == std::string::npos ? "" : s.substr(a, b - a + 1); }
  long toInt() const { return atol(s.c_str()); }
  float toFloat() const { return (float)atof(s.c_str()); }
  bool equalsIgnoreCase(const String &o) const { return strcasecmp(s.c_str(), o.s.c_str()) == 0; }
  void replace(const String &a, const String &b) {
    size_t p = 0;
    while ((p = s.find(a.s, p)) != std::string::npos) { s.replace(p, a.s.size(), b.s); p += b.s.size(); }
  }
  char operator[](unsigned i) const { return i < s.size() ? s[i] : 0; }
  String &operator+=(const String &o) { s += o.s; return *this; }
  String &operator+=(const char *o) { s += o; return *this; }
  String &operator+=(char o) { s += o; return *this; }
  bool operator==(const String &o) const { return s == o.s; }
  bool operator==(const char *o) const { return s == o; }
  bool operator!=(const String &o) const { return s != o.s; }
  bool operator<(const String &o) const { return s < o.s; }
  friend String operator+(const String &a, const String &b) { return String(a.s + b.s); }
  friend String operator+(const String &a, const char *b) { return String(a.s + b); }
  friend String operator+(const char *a, const String &b) { return String(std::string(a) + b.s); }
  friend String operator+(const String &a, char b) { return String(a.s + b); }
};

class Print {};
struct SerialShim {
  template <typename... A> void printf(const char *f, A... a) { ::printf(f, a...); }
  void println(const char *s) { ::puts(s); }
};
static SerialShim Serial;
