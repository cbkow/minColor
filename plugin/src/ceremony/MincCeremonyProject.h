/* Native project ceremonies. lean-v3: Migrate (live config switch to the lean interface config,
   sidecar, self-contained effect rebuild — no reopen) and Repair (same-hash re-point). Set Up,
   Package and Archive were retired (Migrate is the single managed-project entry). */
#pragma once
#include <string>
#include "MincCore.h"

/* JSON report or {"error": "..."} — written verbatim to settings/reports/ */
std::string MincMigrateProject(SPBasicSuite *bp, AEGP_PluginID id, const std::string &presetKey);

/* Native Repair — same-hash re-point via the patch ceremony (shell-less twin of the shell's
   live heal). Green -> action none; no target -> error. */
std::string MincRepairProject(SPBasicSuite *bp, AEGP_PluginID id);
