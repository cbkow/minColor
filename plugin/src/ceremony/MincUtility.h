/* Native utility layers — ensureUtilityLayers/upsertUtilityLayer port (src/minColor.jsxinc
   :1145-1201, :1356-1360): ONE view + ONE render adjustment layer per comp, view guide+on,
   render off when view lands last; effects are VIEW/RENDER VARIANTS (M3: new surface authors
   variants first-class); look partners INHERITED here (set/remove is Apply Look's job).
   Probe U-verified creation path (RESULTS §35).                                            */
#pragma once
#include <string>
#include "MincCore.h"

/* args {view, render} — empty = plugin-menus defaults. JSON report or {"error": ...}. */
std::string MincEnsureUtilityLayers(SPBasicSuite *bp, AEGP_PluginID id,
                                    const std::string &viewSpace, const std::string &renderSpace);

/* shared finder (Apply Look reuses it): the singleton utility layer for a kind, identified
   by an adjustment layer carrying an effect named "minColor: <kind> ...". Returns 1-based
   effect indexes (0 = none).                                                               */
struct MincUtilLayer {
    bool found = false;
    AEGP_LayerH layer = nullptr;
    int fxIndex1 = 0;        /* the kind's transform effect */
    int lookIndex1 = 0;      /* optional "minColor: look ..." partner */
    std::string fxName, lookName;
};
MincUtilLayer MincFindUtilityLayer(SPBasicSuite *bp, AEGP_PluginID id, AEGP_CompH comp, const char *kind);

/* applyLook port (:1216-1236): set/replace/remove the MINC LOOK partner on EXISTING utility
   layers, spaces and enabled states untouched. look "" = remove. JSON report or {"error"}. */
std::string MincApplyLook(SPBasicSuite *bp, AEGP_PluginID id, const std::string &look);

/* applyRenderPreset port: recipe -> both utility layers + look (absent look = remove). */
std::string MincApplyRenderPreset(SPBasicSuite *bp, AEGP_PluginID id, const std::string &name);
