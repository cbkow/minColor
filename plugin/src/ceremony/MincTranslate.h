/* Native plugin→native-dialect translation — the Package half of translateEffects
   (src/minColor.jsxinc:594-627). Headless native popup writes per Probe H (RESULTS §32). */
#pragma once
#include <string>
#include <vector>
#include "MincCore.h"

struct MincTranslateReport {
    std::vector<std::string> converted, skipped, failed, remapped, removed;
};
MincTranslateReport MincTranslateToNative(SPBasicSuite *bp, AEGP_PluginID id);
