/* Main-thread snapshot of AE's current OCIO state via AEGP_ColorSettingsSuite5.
   AEGP suites are main-thread only — render threads read the mutexed snapshot copy.  */
#include "MinColorCST.h"
#include <mutex>
#include <cstring>
#include <cstdio>
#include <pthread.h>

static std::mutex             g_mx;
static MincAuthoritySnapshot  g_snap = {};
static AEGP_PluginID          g_aegpID = 0;
static bool                   g_registered = false;
static bool                   g_hooksOK = false;
static SPBasicSuite          *g_pica = nullptr;

/* M1 diagnostic log — remove once the authority path is proven */
static void AuthLog(const char *fmt, ...) {
    FILE *f = fopen("/tmp/minColorCST_authority.log", "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}

static void Utf16HandleToUtf8(AEGP_SuiteHandler &suites, AEGP_MemHandle h, char *out, size_t outLen) {
    out[0] = '\0';
    if (!h) return;
    A_UTF16Char *p16 = nullptr;
    if (suites.MemorySuite1()->AEGP_LockMemHandle(h, reinterpret_cast<void**>(&p16)) == A_Err_NONE && p16) {
        size_t o = 0;                                   /* BMP-only conversion — config paths/space names are ASCII-ish */
        for (size_t i = 0; p16[i] && o + 4 < outLen; ++i) {
            unsigned c = p16[i];
            if      (c < 0x80)  out[o++] = (char)c;
            else if (c < 0x800) { out[o++] = (char)(0xC0 | (c >> 6));  out[o++] = (char)(0x80 | (c & 0x3F)); }
            else                { out[o++] = (char)(0xE0 | (c >> 12)); out[o++] = (char)(0x80 | ((c >> 6) & 0x3F)); out[o++] = (char)(0x80 | (c & 0x3F)); }
        }
        out[o] = '\0';
        suites.MemorySuite1()->AEGP_UnlockMemHandle(h);
    }
    suites.MemorySuite1()->AEGP_FreeMemHandle(h);
}

static A_Err IdleHook(AEGP_GlobalRefcon, AEGP_IdleRefcon, A_long *max_sleepPL) {
    if (g_pica) {
        PF_InData fake = {};                            /* Refresh only needs pica */
        fake.pica_basicP = g_pica;
        MincAuthorityRefresh(&fake);
    }
    if (max_sleepPL) *max_sleepPL = 60;                 /* ~1 s at 60 ticks */
    return A_Err_NONE;
}

void MincAuthorityGlobalSetup(PF_InData *in_data) {
    if (g_registered) return;
    try {
        g_pica = in_data->pica_basicP;
        AEGP_SuiteHandler suites(in_data->pica_basicP);
        A_Err e = suites.UtilitySuite6()->AEGP_RegisterWithAEGP(NULL, "minColorCST", &g_aegpID);
        g_registered = (e == A_Err_NONE);
        AuthLog("RegisterWithAEGP err=%d id=%d", (int)e, (int)g_aegpID);
        if (g_registered) {
            A_Err e2 = suites.RegisterSuite5()->AEGP_RegisterIdleHook(g_aegpID, IdleHook, NULL);
            g_hooksOK = (e2 == A_Err_NONE);
            AuthLog("RegisterIdleHook err=%d", (int)e2);   /* M1 decision point: single binary vs sibling AEGP */
        }
    } catch (...) { AuthLog("GlobalSetup: exception"); }
    MincAuthorityRefresh(in_data);
}

void MincAuthorityRefresh(PF_InData *in_data) {
    if (!pthread_main_np()) return;                     /* AEGP suites: main thread only */
    if (!g_registered || !in_data || !in_data->pica_basicP) return;
    try {
        AEGP_SuiteHandler suites(in_data->pica_basicP);
        const void *csV = nullptr;
        if (in_data->pica_basicP->AcquireSuite(kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion5,
                                               &csV) != kSPNoError || !csV) return;
        const AEGP_ColorSettingsSuite5 *cs = reinterpret_cast<const AEGP_ColorSettingsSuite5*>(csV);
        MincAuthoritySnapshot next = {};
        A_Boolean on = FALSE;
        cs->AEGP_IsOCIOColorManagementUsed(g_aegpID, &on);
        next.ocioOn = (on != FALSE);
        if (next.ocioOn) {
            AEGP_MemHandle pathH = nullptr, wsH = nullptr;
            if (cs->AEGP_GetOCIOConfigurationFilePath(g_aegpID, &pathH) == A_Err_NONE)
                Utf16HandleToUtf8(suites, pathH, next.configPath, sizeof(next.configPath));
            if (cs->AEGPD_GetOCIOWorkingColorSpace(g_aegpID, &wsH) == A_Err_NONE)
                Utf16HandleToUtf8(suites, wsH, next.workingSpace, sizeof(next.workingSpace));
        }
        in_data->pica_basicP->ReleaseSuite(kAEGPColorSettingsSuite, kAEGPColorSettingsSuiteVersion5);
        std::lock_guard<std::mutex> lk(g_mx);
        if (next.ocioOn != g_snap.ocioOn ||
            strcmp(next.configPath, g_snap.configPath) != 0 ||
            strcmp(next.workingSpace, g_snap.workingSpace) != 0) {
            next.generation = g_snap.generation + 1;
            g_snap = next;
            AuthLog("snapshot gen=%lu ocioOn=%d config=%s working=%s",
                    g_snap.generation, (int)g_snap.ocioOn, g_snap.configPath, g_snap.workingSpace);
        }
    } catch (...) { AuthLog("Refresh: exception"); }
}

bool MincAuthorityGet(MincAuthoritySnapshot *out) {
    std::lock_guard<std::mutex> lk(g_mx);
    *out = g_snap;
    return g_snap.generation > 0;
}
