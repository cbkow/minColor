/* Native Archive — port of archiveProject (src/minColor.jsxinc:499-532): sidecar (config +
   luts/filmic/icc trees + minColor.json v2 merge), provenance.json from the saved file's XMP.
   Golden reference render deferred to M2 (native reports it skipped; scenario 18 excludes the
   golden field from the diff by design). Never touches the pin (only Package re-pins).       */
#pragma once
#include <string>
#include "MincCore.h"


/* ensureSidecar core (also used by Package): trees + config beside projPath + minColor.json
   merge. cfgOut = the sidecar config's absolute path. */
bool MincEnsureSidecar(const std::string &projPath, const std::string &preset,
                       std::string *cfgOut, std::string *errOut);

/* lean-v3 Path 2: generate the per-preset lean INTERFACE config into _minColor and return its
   path. This is what AE PINS (its own neutralizer) — the effect never reads it. It exposes only
   the preset's working space (scene_linear = default_* = that space, so the working dropdown is
   locked and UNASSIGNED footage passes through untouched) plus one passthrough Raw view. The
   effect renders from the full config (config-<preset>.ocio, embedded / central store). Basename
   convention: config-<preset>-interface.ocio, so authoring strips "-interface" to name the full
   config in the passport. */
bool MincWriteInterfaceConfig(const std::string &projPath, const std::string &preset,
                              std::string *ifaceOut, std::string *errOut);
