/* plugin-menus.json writer — AEGP target. The AEGP derives the file from presets.json +
   the pinned config (ordered menuLists port in MincSuggest); the panel never learns about
   it. Written temp-then-rename after Migrate / Repair, and by the Doctor whenever the file
   is missing or names another preset (2026-09-06). No-op (false) when the project isn't a
   minColor project.                                                                      */
#pragma once
#include <string>
#include "MincCore.h"

bool MincWriteMenus(SPBasicSuite *bp, AEGP_PluginID id);
bool MincWriteMenusForPreset(const std::string &preset);   /* from the preset alone (Doctor path) */
bool MincMenusFileIsFor(const std::string &preset);        /* plugin-menus.json exists and names this preset */
