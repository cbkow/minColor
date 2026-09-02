/* Main-thread snapshot of AE's current OCIO state via AEGP_ColorSettingsSuite5.
   AEGP suites are main-thread only — render threads read the mutexed snapshot copy.
   PER-BINARY instance: the effect refreshes for render/badge, the AEGP for walk passports
   and the generation watch. The two snapshots may briefly disagree; both converge on the
   same live AE state within a tick.                                                       */
#include "MincCore.h"
#include <mutex>
#include <cstring>
#ifndef AE_OS_WIN
#include <pthread.h>
#endif

static std::mutex             g_mx;
static MincAuthoritySnapshot  g_snap = {};
#ifdef AE_OS_WIN
static DWORD                  g_mainTid = 0;            /* stands in for pthread_main_np */
#endif

void MincSetMainThread(void) {
#ifdef AE_OS_WIN
    g_mainTid = GetCurrentThreadId();
#endif
}

void MincAuthorityRefreshBp(SPBasicSuite *bp, AEGP_PluginID aegpId) {
#ifdef AE_OS_WIN
    if (GetCurrentThreadId() != g_mainTid) return;      /* AEGP suites: main thread only */
#else
    if (!pthread_main_np()) return;                     /* AEGP suites: main thread only */
#endif
    if (!bp || !aegpId) return;
    try {
        AEGP_SuiteHandler suites(bp);
        const void *csV = nullptr;
        if (bp->AcquireSuite(kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion5,
                             &csV) != kSPNoError || !csV) return;
        const AEGP_ColorSettingsSuite5 *cs = reinterpret_cast<const AEGP_ColorSettingsSuite5*>(csV);
        MincAuthoritySnapshot next = {};
        A_Boolean on = FALSE;
        cs->AEGP_IsOCIOColorManagementUsed(aegpId, &on);
        next.ocioOn = (on != FALSE);
        if (next.ocioOn) {
            AEGP_MemHandle pathH = nullptr, wsH = nullptr;
            if (cs->AEGP_GetOCIOConfigurationFilePath(aegpId, &pathH) == A_Err_NONE)
                MincUtf16HandleToUtf8(suites, pathH, next.configPath, sizeof(next.configPath));
            if (cs->AEGPD_GetOCIOWorkingColorSpace(aegpId, &wsH) == A_Err_NONE)
                MincUtf16HandleToUtf8(suites, wsH, next.workingSpace, sizeof(next.workingSpace));
        }
        bp->ReleaseSuite(kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion5);
        std::lock_guard<std::mutex> lk(g_mx);
        if (next.ocioOn != g_snap.ocioOn ||
            strcmp(next.configPath, g_snap.configPath) != 0 ||
            strcmp(next.workingSpace, g_snap.workingSpace) != 0) {
            next.generation = g_snap.generation + 1;
            g_snap = next;
            MincLog("snapshot gen=%lu ocioOn=%d config=%s working=%s",
                    g_snap.generation, (int)g_snap.ocioOn, g_snap.configPath, g_snap.workingSpace);
        }
    } catch (...) { MincLog("Refresh: exception"); }
}

bool MincAuthorityGet(MincAuthoritySnapshot *out) {
    std::lock_guard<std::mutex> lk(g_mx);
    *out = g_snap;
    return g_snap.generation > 0;
}
