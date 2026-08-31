/* plugin-menus.json writer — AEGP target. The AEGP derives the file from presets.json +
   the pinned config (ordered menuLists port in MincSuggest); the panel never learns about
   it. Written temp-then-rename at launch/generation-change (idle) and after Set Up /
   Migrate / Package. No-op (false) when the project isn't a minColor project.            */
#pragma once
#include <string>
#include "MincCore.h"

bool MincWriteMenus(SPBasicSuite *bp, AEGP_PluginID id);
