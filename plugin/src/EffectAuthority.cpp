/* Effect-binary AEGP registration — 2.0: the menu command, update-menu hook, and the
   generation-watch auto-walk moved to the minColorAEGP bundle (startup-registered). The
   effect keeps ONLY:
   - RegisterWithAEGP for its plugin id (render/badge refresh; M2's badge-edit CallGeneric),
   - a refresh-ONLY idle hook: render and the badge read this binary's authority snapshot on
     a ~1s cadence (SmartRender/Ui) — dropping it would leave them stale after a pin or
     working-space change that produces no param events (M0 validation gap G1).             */
#include "MinColorCST.h"

static AEGP_PluginID  g_aegpID = 0;
static bool           g_registered = false;
static SPBasicSuite  *g_pica = nullptr;

static A_Err IdleHook(AEGP_GlobalRefcon, AEGP_IdleRefcon, A_long *max_sleepPL) {
    if (g_pica && g_registered) MincAuthorityRefreshBp(g_pica, g_aegpID);
    if (max_sleepPL) *max_sleepPL = 60;                 /* ~1 s at 60 ticks */
    return A_Err_NONE;
}

void MincAuthorityGlobalSetup(PF_InData *in_data) {
    if (g_registered) return;
    MincSetMainThread();                                /* GLOBAL_SETUP runs on the main thread */
    MincLog("boot: effect GlobalSetup enter @%.0fms", MincNowMs());
    try {
        g_pica = in_data->pica_basicP;
        AEGP_SuiteHandler suites(in_data->pica_basicP);
        A_Err e = suites.UtilitySuite6()->AEGP_RegisterWithAEGP(NULL, "minColorCST", &g_aegpID);
        g_registered = (e == A_Err_NONE);
        MincLog("RegisterWithAEGP err=%d id=%d", (int)e, (int)g_aegpID);
        if (g_registered) {
            A_Err e2 = suites.RegisterSuite5()->AEGP_RegisterIdleHook(g_aegpID, IdleHook, NULL);
            MincLog("RegisterIdleHook (refresh-only) err=%d", (int)e2);
        }
    } catch (...) { MincLog("GlobalSetup: exception"); }
    MincLog("boot: effect GlobalSetup exit @%.0fms", MincNowMs());
    /* no authority refresh here: the idle hook does it within ~1s, and suite calls during the
       startup plugin scan are a boot-time cost we do not need to pay */
}

void MincAuthorityRefresh(PF_InData *in_data) {
    if (!g_registered || !in_data || !in_data->pica_basicP) return;
    MincAuthorityRefreshBp(in_data->pica_basicP, g_aegpID);
}
