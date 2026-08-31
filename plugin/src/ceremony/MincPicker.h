/* Spartan preset picker. Quiet mode reads settings/quiet-answers.json {"preset": "..."};
   interactive shows an NSAlert with a preset popup (MincPicker.mm).                     */
#pragma once
#include <string>

bool MincPickPreset(std::string *keyOut);
