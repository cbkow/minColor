/* Effect operations for the ceremonies — minColorAEGP target only. M1 Step 2 grows this;
   the installed-key cache lands first (with a timed scan answering the iterate-vs-keyed
   question empirically on this machine's real effect list).                              */
#pragma once
#include "MincCore.h"

#include <string>
#include <vector>

/* Find (once, cached) the installed-effect key for MINC_MATCH_LEGACY by iterating
   AEGP_GetNextInstalledEffect. First call logs entry count + duration. 0 = not installed. */
AEGP_InstalledEffectKey MincInstalledKey(SPBasicSuite *bp, AEGP_PluginID id);

struct MincFxEntry { std::string match, name; };              /* parade order */
bool MincEnumLayerEffects(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH,
                          std::vector<MincFxEntry> *out);

/* apply the XFORM variant + set the grammar display name (M3 authoring swap — was legacy
   MINC CST); effect lands at the END of the parade. targetIndex1 (1-based) moves it when
   > 0 and different. */
bool MincApplyMincWithName(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH,
                           const std::string &grammarName, int targetIndex1);

/* lean-v3 self-contained authoring: apply the XFORM variant AND write its arb (space + passport)
   through the just-applied ref, before dispose/move (§34 ordering) — so the saved project carries
   the render-truth with no name-walk. MincReauthorEffectAt writes the arb onto an effect already
   at a 1-based parade index (after a rename-in-place). */
bool MincApplyMincSelfContained(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH,
                                const std::string &grammarName, int targetIndex1,
                                const MinColorArb *arb, const char *configBase, const char *working);
bool MincReauthorEffectAt(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH, int index1,
                          const MinColorArb *arb, const char *configBase, const char *working);

bool MincRenameEffectAt(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH,
                        int index1, const std::string &newName);
bool MincRemoveEffectAt(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH, int index1);
bool MincMoveEffect(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH, int fromIndex1, int toIndex1);

bool MincLayerLocked(SPBasicSuite *bp, AEGP_LayerH layerH);
void MincSetLayerLocked(SPBasicSuite *bp, AEGP_LayerH layerH, bool locked);

/* generic apply-by-match (cached key lookup per match name) + rename. The effect lands at
   the parade END and STAYS there (appliedIndex1 reports the 1-based slot): the returned ref
   resolves by parade position, so reordering before the caller's stream writes rebinds it
   to the WRONG effect (M2 crash of record — popup value written into a MINC arb stream).
   Finish all writes through the ref, dispose it, THEN MincMoveEffect into place.
   Caller MUST dispose. nullptr on failure.                                                */
AEGP_EffectRefH MincApplyByMatchWithName(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH,
                                         const char *matchName, const std::string &dispName,
                                         int *appliedIndex1);

/* popup-by-name (setPopupByName :1086-1094 semantics, HEADLESS — Probe H, RESULTS §32):
   exact match, then "wanted: " role-prefix, then "/wanted" or ": wanted" suffix.
   Writes 1-based index via SetStreamValue one_d. */
bool MincSetPopupByName(SPBasicSuite *bp, AEGP_PluginID id, AEGP_EffectRefH effH,
                        int paramIdx, const std::string &wanted, std::string *errOut);
