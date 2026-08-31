/* Native Archive — port of archiveProject (src/minColor.jsxinc:499-532): sidecar (config +
   luts/filmic/icc trees + minColor.json v2 merge), provenance.json from the saved file's XMP.
   Golden reference render deferred to M2 (native reports it skipped; scenario 18 excludes the
   golden field from the diff by design). Never touches the pin (only Package re-pins).       */
#pragma once
#include <string>
#include "MincCore.h"

std::string MincArchiveProject(SPBasicSuite *bp, AEGP_PluginID id);   /* JSON report */
