/* Effect operations for the ceremonies — minColorAEGP target only. M1 Step 2 grows this;
   the installed-key cache lands first (with a timed scan answering the iterate-vs-keyed
   question empirically on this machine's real effect list).                              */
#pragma once
#include "MincCore.h"

#include <string>
#include <vector>

/* Find (once, cached) the installed-effect key for MINC_MATCH_NAME by iterating
   AEGP_GetNextInstalledEffect. First call logs entry count + duration. 0 = not installed. */
AEGP_InstalledEffectKey MincInstalledKey(SPBasicSuite *bp, AEGP_PluginID id);

struct MincFxEntry { std::string match, name; };              /* parade order */
bool MincEnumLayerEffects(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH,
                          std::vector<MincFxEntry> *out);

/* apply MINC CST + set the grammar display name; effect lands at the END of the parade.
   targetIndex1 (1-based) moves it when > 0 and different. */
bool MincApplyMincWithName(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH,
                           const std::string &grammarName, int targetIndex1);

bool MincRenameEffectAt(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH,
                        int index1, const std::string &newName);
bool MincRemoveEffectAt(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH, int index1);
bool MincMoveEffect(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH, int fromIndex1, int toIndex1);

bool MincLayerLocked(SPBasicSuite *bp, AEGP_LayerH layerH);
void MincSetLayerLocked(SPBasicSuite *bp, AEGP_LayerH layerH, bool locked);
