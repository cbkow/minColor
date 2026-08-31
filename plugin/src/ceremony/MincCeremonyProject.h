/* Native project ceremonies — Set Up (applyPresetToCurrent port, :670-680) and Migrate
   (:724-770), zero-bridge: save → RIFX patch (pin+cms, pwcsJSON, footage strips, XMP
   upserts) → reopen → bit depth → rebuild walk. Report shapes mirror the panel exactly
   (scenario 15/16 diff them literally after normalization).                             */
#pragma once
#include <string>
#include "MincCore.h"

/* JSON report or {"error": "..."} — written verbatim to settings/reports/ */
std::string MincApplyPresetToCurrent(SPBasicSuite *bp, AEGP_PluginID id, const std::string &presetKey);
std::string MincMigrateProject(SPBasicSuite *bp, AEGP_PluginID id, const std::string &presetKey);

/* Package for Any AE — packageForAnyAE port (:490-496): translate plugin→native, re-pin to
   the sidecar config (patch ceremony; the panel re-pins live), engine stamp, archive. */
std::string MincPackageForAnyAE(SPBasicSuite *bp, AEGP_PluginID id);
