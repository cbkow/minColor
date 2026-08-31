/* Native Interpret Timeline — verbatim port of interpretPass({mode:"comp"}) (src/minColor.jsxinc
   :946-1083). Report labels must match the panel byte-for-byte (equivalence scenario 14).     */
#pragma once
#include <string>
#include <vector>
#include "MincCore.h"

struct MincInterpretReport {
    std::vector<std::string> added, skipped, flagged, failed, identity, contained;
    std::string error;                                   /* nonempty = the pass threw (e.g. no active comp) */
    std::string toJson(void) const;
};

MincInterpretReport MincInterpretTimeline(SPBasicSuite *bp, AEGP_PluginID id);
