#include "todo.h"
#include <ArduinoJson.h>
#include <SD.h>

struct Item {
  String text;
  bool done = false;
};
static std::vector<Item> items;
static const char *PATH = "/todo.json";

void todoLoad() {
  items.clear();
  File f = SD.open(PATH);
  if (!f) return;
  JsonDocument d;
  if (!deserializeJson(d, f))
    for (JsonObjectConst o : d.as<JsonArrayConst>()) items.push_back({o["t"] | "", o["d"] | false});
  f.close();
}

static bool save() {
  JsonDocument d;
  for (auto &i : items) {
    JsonObject o = d.add<JsonObject>();
    o["t"] = i.text;
    o["d"] = i.done;
  }
  File f = SD.open(PATH, FILE_WRITE);
  if (!f) return false;
  serializeJson(d, f);
  f.close();
  return true;
}

std::vector<String> todoOpen() {
  std::vector<String> out;
  for (auto &i : items)
    if (!i.done) out.push_back(i.text);
  return out;
}

String todoJson() {
  JsonDocument d;
  d.to<JsonArray>();
  for (auto &i : items) {
    JsonObject o = d.add<JsonObject>();
    o["t"] = i.text;
    o["d"] = i.done;
  }
  String s;
  serializeJson(d, s);
  return s;
}

bool todoAdd(const String &text) {
  String t = text;
  t.trim();
  if (!t.length() || items.size() >= 30) return false;
  items.push_back({t.substring(0, 80), false});
  return save();
}

bool todoToggle(int i) {
  if (i < 0 || i >= (int)items.size()) return false;
  items[i].done = !items[i].done;
  return save();
}

bool todoRemove(int i) {
  if (i < 0 || i >= (int)items.size()) return false;
  items.erase(items.begin() + i);
  return save();
}

bool todoClearDone() {
  std::vector<Item> keep;
  for (auto &i : items)
    if (!i.done) keep.push_back(i);
  items = keep;
  return save();
}
