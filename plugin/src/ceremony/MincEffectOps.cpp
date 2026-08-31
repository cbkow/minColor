#include "MincEffectOps.h"
#include <cstring>

AEGP_InstalledEffectKey MincInstalledKey(SPBasicSuite *bp, AEGP_PluginID id) {
    static AEGP_InstalledEffectKey cached = 0;
    static bool scanned = false;
    if (scanned) return cached;
    scanned = true;
    Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5);
    if (!efs) { MincLog("effect-key scan: suite acquire failed"); return 0; }
    double t0 = MincNowMs();
    int n = 0;
    AEGP_InstalledEffectKey k = AEGP_InstalledEffectKey_NONE, next = AEGP_InstalledEffectKey_NONE;
    while (efs->AEGP_GetNextInstalledEffect(k, &next) == A_Err_NONE && next != AEGP_InstalledEffectKey_NONE) {
        ++n;
        A_char mn[64] = "";
        if (efs->AEGP_GetEffectMatchName(next, mn) == A_Err_NONE && !strcmp(mn, MINC_MATCH_NAME)) cached = next;
        k = next;
    }
    double dt = MincNowMs() - t0;
    /* the iterate-vs-keyed-lookup answer, on the record (plan: M1 step 2) */
    MincLog("effect-key scan: %d installed effects in %.2f ms; MINC CST key=%d", n, dt, (int)cached);
    (void)id;
    return cached;
}
