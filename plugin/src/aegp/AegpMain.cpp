/* minColorAEGP — the ceremonies bundle. Kind{AEGP}/AEgx, lives in the APP's Plug-ins folder,
   loads at AE LAUNCH (the effect's GLOBAL_SETUP is deferred until first instantiation and
   MediaCore never loads AEGPs — probes D/F, RESULTS.md §30b/c). 2.0 M0 scope: exactly what the
   1.x effect used to register — "minColor: Sync From Names" + the generation-watch auto-walk —
   so the 0.9.2 panel sees identical behavior, just available from startup.                   */
#include "MincCore.h"

static AEGP_PluginID  g_id = 0;
static SPBasicSuite  *g_pica = nullptr;
static AEGP_Command   g_syncCmd = 0;

AEGP_InstalledEffectKey MincInstalledKey(SPBasicSuite *bp, AEGP_PluginID id);   /* ceremony/MincEffectOps.cpp */

static A_Err IdleHook(AEGP_GlobalRefcon, AEGP_IdleRefcon, A_long *max_sleepPL) {
    static unsigned long lastSyncedGen = 0;
    static bool keyScanned = false;
    if (g_pica && g_id) {
        if (!keyScanned) { keyScanned = true; MincInstalledKey(g_pica, g_id); }   /* timed scan, once, post-startup */
        MincAuthorityRefreshBp(g_pica, g_id);
        MincAuthoritySnapshot s;                        /* project/preset changed -> names are the durable
                                                           store: re-derive every instance's state */
        if (MincAuthorityGet(&s) && s.generation != lastSyncedGen) {
            lastSyncedGen = s.generation;
            MincSyncFromNames(g_pica, g_id);
        }
    }
    if (max_sleepPL) *max_sleepPL = 60;                 /* ~1 s at 60 ticks */
    return A_Err_NONE;
}

static A_Err CommandHook(AEGP_GlobalRefcon, AEGP_CommandRefcon, AEGP_Command command,
                         AEGP_HookPriority, A_Boolean, A_Boolean *handledPB) {
    if (command == g_syncCmd && g_pica) {
        MincAuthorityRefreshBp(g_pica, g_id);           /* refresh FIRST — the sync payload carries
                                                           authority state (idle hook does this order) */
        MincSyncFromNames(g_pica, g_id);
        if (handledPB) *handledPB = TRUE;
    }
    return A_Err_NONE;
}

static A_Err UpdateMenuHook(AEGP_GlobalRefcon, AEGP_UpdateMenuRefcon, AEGP_WindowType) {
    if (g_pica && g_syncCmd) {
        Acq<AEGP_CommandSuite1> cs(g_pica, kAEGPCommandSuite, kAEGPCommandSuiteVersion1);
        if (cs) cs->AEGP_EnableCommand(g_syncCmd);
    }
    return A_Err_NONE;
}

extern "C" DllExport A_Err MincAegpEntry(struct SPBasicSuite *pica_basicP, A_long major, A_long minor,
                                         AEGP_PluginID aegp_plugin_id, AEGP_GlobalRefcon *global_refconP) {
    g_pica = pica_basicP;
    g_id = aegp_plugin_id;
    if (global_refconP) *global_refconP = nullptr;
    MincSetMainThread();
    MincLog("boot: minColorAEGP entry at launch — host %ld.%ld id=%d (%s)",
            (long)major, (long)minor, (int)aegp_plugin_id, MINC_BUILD_STAMP);
    A_Err out = A_Err_NONE;
    try {
        AEGP_SuiteHandler suites(pica_basicP);
        A_Err e2 = suites.RegisterSuite5()->AEGP_RegisterIdleHook(g_id, IdleHook, NULL);
        Acq<AEGP_CommandSuite1> cs(pica_basicP, kAEGPCommandSuite, kAEGPCommandSuiteVersion1);
        if (cs && cs->AEGP_GetUniqueCommand(&g_syncCmd) == A_Err_NONE && g_syncCmd) {
            cs->AEGP_InsertMenuCommand(g_syncCmd, "minColor: Sync From Names", AEGP_Menu_EDIT, AEGP_MENU_INSERT_AT_BOTTOM);
            A_Err e3 = suites.RegisterSuite5()->AEGP_RegisterCommandHook(g_id, AEGP_HP_BeforeAE, g_syncCmd, CommandHook, NULL);
            A_Err e4 = suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(g_id, UpdateMenuHook, NULL);
            MincLog("sync command=%d idle=%d hook=%d menuhook=%d", (int)g_syncCmd, (int)e2, (int)e3, (int)e4);
        } else { MincLog("sync command registration FAILED"); out = A_Err_GENERIC; }
    } catch (...) { MincLog("AegpEntry: exception"); out = A_Err_GENERIC; }
    return out;
}
