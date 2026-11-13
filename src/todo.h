// todo.h - a small to-do list kept in /todo.json on the microSD card, edited from the phone page.
#pragma once
#include <Arduino.h>
#include <vector>

void todoLoad();
std::vector<String> todoOpen();   // texts of items not yet done
String todoJson();                // [{"t":"Call Sam","d":false},...] for the phone page
bool todoAdd(const String &text);
bool todoToggle(int index);
bool todoRemove(int index);
bool todoClearDone();
