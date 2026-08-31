/* Effect operations for the ceremonies — minColorAEGP target only. M1 Step 2 grows this;
   the installed-key cache lands first (with a timed scan answering the iterate-vs-keyed
   question empirically on this machine's real effect list).                              */
#pragma once
#include "MincCore.h"

/* Find (once, cached) the installed-effect key for MINC_MATCH_NAME by iterating
   AEGP_GetNextInstalledEffect. First call logs entry count + duration. 0 = not installed. */
AEGP_InstalledEffectKey MincInstalledKey(SPBasicSuite *bp, AEGP_PluginID id);
