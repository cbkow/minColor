/* Native Doctor — diagnose only (M1; repair stays panel-side). Verbatim port of doctor() +
   sidecarInfo() (src/minColor.jsxinc:199-271) so the equivalence diff is byte-level.       */
#pragma once
#include <string>
#include "MincCore.h"

struct MincDoctorResult {
    std::string status;        /* "green" | "yellow" | "red" | "unmanaged" */
    std::string text;
    bool        canRepair = false;
    std::string preset;        /* "" == null */
    std::string family = "Linear";
    std::string pin;           /* basename */
    bool        behind = false;
    std::string behindPinned, behindCurrent;
    std::string toJson(void) const;
};

MincDoctorResult MincDoctorDiagnose(SPBasicSuite *bp, AEGP_PluginID id);
