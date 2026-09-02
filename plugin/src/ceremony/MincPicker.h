/* Spartan preset picker, three-tier: shell-args {"command": <label>, "preset": ...} first
   (the M3 shell's dropdown), then quiet-answers.json under quiet mode, then the NSAlert
   popup (MincPicker.mm) — the NSAlert stays the menu-direct fallback forever.           */
#pragma once
#include <string>

bool MincPickPreset(const char *commandLabel, std::string *keyOut);
