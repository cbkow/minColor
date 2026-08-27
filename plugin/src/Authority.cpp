/* Main-thread snapshot of AE's current OCIO state via AEGP_ColorSettingsSuite5.
   AEGP suites are main-thread only — render threads read the mutexed snapshot copy.  */
#include "MinColorCST.h"
#include <mutex>
#include <map>
#include <cstring>
#include <cstdio>
#ifndef AE_OS_WIN
#include <pthread.h>
#endif

static std::mutex             g_regMx;
static std::map<uint32_t, MincSeqData> g_registry;
void MincRegistrySet(uint32_t id, const MincSeqData *sd) {
    if (!id) return;
    std::lock_guard<std::mutex> lk(g_regMx);
    g_registry[id] = *sd;
}
bool MincRegistryGet(uint32_t id, MincSeqData *out) {
    if (!id) return false;
    std::lock_guard<std::mutex> lk(g_regMx);
    auto it = g_registry.find(id);
    if (it == g_registry.end()) return false;
    *out = it->second;
    return true;
}

static std::mutex             g_mx;
static MincAuthoritySnapshot  g_snap = {};
static AEGP_PluginID          g_aegpID = 0;
static bool                   g_registered = false;
static bool                   g_hooksOK = false;
static SPBasicSuite          *g_pica = nullptr;

#ifdef AE_OS_WIN
#include <cstdlib>
#include <chrono>
static const char *MincLogPath() {                                  /* %TEMP%/minColorCST_authority.log */
    static char p[MAX_PATH + 40] = "";
    if (!p[0]) { const char *t = getenv("TEMP"); snprintf(p, sizeof(p), "%s/minColorCST_authority.log", t ? t : "C:/Windows/Temp"); }
    return p;
}
#define MINC_LOG_PATH MincLogPath()
static double NowMs() { return std::chrono::duration<double, std::milli>(std::chrono::system_clock::now().time_since_epoch()).count(); }
static struct MincLoadStampT {                                      /* DLL load moment: catches static-init cost */
    MincLoadStampT() { FILE *f = fopen(MINC_LOG_PATH, "a"); if (f) { fprintf(f, "boot: dll loaded @%.0fms\n", NowMs()); fclose(f); } }
} s_mincLoadStamp;
static DWORD g_mainTid = 0;                                         /* stands in for pthread_main_np: set at GlobalSetup */
#else
#define MINC_LOG_PATH "/tmp/minColorCST_authority.log"
#include <sys/time.h>
static double NowMs() { struct timeval tv; gettimeofday(&tv, nullptr); return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0; }
__attribute__((constructor)) static void MincLoadStamp() {          /* dylib load moment: catches static-init cost */
    FILE *f = fopen(MINC_LOG_PATH, "a");
    if (f) { fprintf(f, "boot: dylib loaded @%.0fms\n", NowMs()); fclose(f); }
}
#endif
void MincDebugLog(const char *fmt, ...) {
#ifndef MINC_DEBUG
    (void)fmt; return;                                   /* hot paths call this per frame/resolve —
                                                            fopen-per-line only in debug builds */
#else
    FILE *f = fopen(MINC_LOG_PATH, "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
#endif
}
/* M1 diagnostic log — remove once the authority path is proven */
static void AuthLog(const char *fmt, ...) {
    FILE *f = fopen(MINC_LOG_PATH, "a");
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

static void SyncFromNames(SPBasicSuite *bp);
static A_Err IdleHook(AEGP_GlobalRefcon, AEGP_IdleRefcon, A_long *max_sleepPL) {
    static unsigned long lastSyncedGen = 0;
    if (g_pica) {
        PF_InData fake = {};                            /* Refresh only needs pica */
        fake.pica_basicP = g_pica;
        MincAuthorityRefresh(&fake);
        MincAuthoritySnapshot s;                        /* project/preset changed -> names are the durable
                                                           store: re-derive every instance's state */
        if (MincAuthorityGet(&s) && s.generation != lastSyncedGen) {
            lastSyncedGen = s.generation;
            SyncFromNames(g_pica);
        }
    }
    if (max_sleepPL) *max_sleepPL = 60;                 /* ~1 s at 60 ticks */
    return A_Err_NONE;
}

/* ---------------- "minColor: Sync From Names" ----------------
   The panel names MINC CST instances ("minColor: X \xe2\x86\x92 working" / "minColor: view X" /
   "minColor: render X") and invokes this menu command; we parse every instance's display name
   and write its arb param. AEGP CAN set arbitrary streams; ExtendScript cannot.            */
static AEGP_Command g_syncCmd = 0;

static bool ParseGrammar(const char *utf8, MinColorArb *out) {
    const char *PRE = "minColor: ";
    if (strncmp(utf8, PRE, 10) != 0) return false;
    const char *rest = utf8 + 10;
    memset(out, 0, sizeof(*out));
    out->magic = MINC_ARB_MAGIC; out->version = MINC_ARB_VERSION;
    if (!strncmp(rest, "view ", 5))   { out->direction = MINC_DIR_FROM_WORKING; snprintf(out->space, MINC_SPACE_LEN, "%s", rest + 5); return true; }
    if (!strncmp(rest, "render ", 7)) { out->direction = MINC_DIR_FROM_WORKING; snprintf(out->space, MINC_SPACE_LEN, "%s", rest + 7); return true; }
    const char *arrow = strstr(rest, " \xe2\x86\x92 working");        /* " -> working", real arrow */
    if (!arrow) arrow = strstr(rest, " -> working");
    if (!arrow) return false;
    out->direction = MINC_DIR_TO_WORKING;
    size_t n = (size_t)(arrow - rest); if (n >= MINC_SPACE_LEN) n = MINC_SPACE_LEN - 1;
    memcpy(out->space, rest, n);
    return true;
}

template <typename SUITE>
struct Acq {                                                        /* tiny RAII suite acquire */
    SPBasicSuite *pica; const char *name; A_long ver; SUITE *p = nullptr;
    Acq(SPBasicSuite *b, const char *n, A_long v) : pica(b), name(n), ver(v) {
        const void *vp = nullptr;
        if (b->AcquireSuite(n, v, &vp) == kSPNoError) p = (SUITE*)const_cast<void*>(vp);
    }
    ~Acq() { if (p) pica->ReleaseSuite(name, ver); }
    SUITE *operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
};

static void SyncFromNames(SPBasicSuite *bp) {
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ProjSuite6>          pjs(bp, kAEGPProjSuite,          kAEGPProjSuiteVersion6);
    Acq<AEGP_ItemSuite9>          its(bp, kAEGPItemSuite,          kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12>         cps(bp, kAEGPCompSuite,          kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9>         lys(bp, kAEGPLayerSuite,         kAEGPLayerSuiteVersion9);
    Acq<AEGP_DynamicStreamSuite4> dss(bp, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4);
    Acq<AEGP_StreamSuite6>        sts(bp, kAEGPStreamSuite,        kAEGPStreamSuiteVersion6);
    Acq<AEGP_EffectSuite5>        efs(bp, kAEGPEffectSuite,        kAEGPEffectSuiteVersion5);
    if (!pjs || !its || !cps || !lys || !dss || !sts || !efs) { AuthLog("sync: suite acquire failed"); return; }
    /* passport source: the CURRENT healthy authority. Written into every synced instance so the
       .aep carries the content-addressed config basename + working space across platforms. */
    MincAuthoritySnapshot snap = {}; MincAuthorityGet(&snap);
    bool authHealthy = snap.ocioOn && snap.configPath[0] && snap.workingSpace[0];
    char passBase[MINC_CONFIGBASE_LEN] = "";
    if (authHealthy) {
        const char *b = snap.configPath;
        for (const char *p = snap.configPath; *p; ++p) if (*p == '/' || *p == '\\') b = p + 1;
        if (b[0] && !strstr(b, "..") && strlen(b) < sizeof(passBase)) strncpy(passBase, b, sizeof(passBase) - 1);
    }
    int seen = 0, wrote = 0, badname = 0;
    AEGP_ProjectH projH = nullptr;
    if (pjs->AEGP_GetProjectByIndex(0, &projH) != A_Err_NONE || !projH) return;
    AEGP_ItemH itemH = nullptr;
    its->AEGP_GetFirstProjItem(projH, &itemH);
    while (itemH) {
        AEGP_ItemType ty = AEGP_ItemType_NONE;
        its->AEGP_GetItemType(itemH, &ty);
        if (ty == AEGP_ItemType_COMP) {
            AEGP_CompH compH = nullptr;
            if (cps->AEGP_GetCompFromItem(itemH, &compH) == A_Err_NONE && compH) {
                A_long nL = 0; lys->AEGP_GetCompNumLayers(compH, &nL);
                for (A_long li = 0; li < nL; ++li) {
                    AEGP_LayerH layerH = nullptr;
                    if (lys->AEGP_GetCompLayerByIndex(compH, li, &layerH) != A_Err_NONE || !layerH) continue;
                    AEGP_StreamRefH layerRef = nullptr, fxGroup = nullptr;
                    if (dss->AEGP_GetNewStreamRefForLayer(g_aegpID, layerH, &layerRef) != A_Err_NONE || !layerRef) continue;
                    {   /* find the effects group by ITERATION — ByMatchname raises an internal
                           verification failure on layers without one (cameras, lights) */
                        A_long nkids = 0;
                        if (dss->AEGP_GetNumStreamsInGroup(layerRef, &nkids) == A_Err_NONE) {
                            for (A_long ki = 0; ki < nkids && !fxGroup; ++ki) {
                                AEGP_StreamRefH kid = nullptr;
                                if (dss->AEGP_GetNewStreamRefByIndex(g_aegpID, layerRef, ki, &kid) != A_Err_NONE || !kid) continue;
                                A_char km[AEGP_MAX_STREAM_MATCH_NAME_SIZE] = "";
                                dss->AEGP_GetMatchName(kid, km);
                                if (!strcmp(km, "ADBE Effect Parade")) fxGroup = kid;
                                else sts->AEGP_DisposeStream(kid);
                            }
                        }
                    }
                    if (fxGroup) {
                        A_long nfx = 0; dss->AEGP_GetNumStreamsInGroup(fxGroup, &nfx);
                        for (A_long fi = 0; fi < nfx; ++fi) {
                            AEGP_StreamRefH fxRef = nullptr;
                            if (dss->AEGP_GetNewStreamRefByIndex(g_aegpID, fxGroup, fi, &fxRef) != A_Err_NONE || !fxRef) continue;
                            A_char match[AEGP_MAX_STREAM_MATCH_NAME_SIZE] = "";
                            dss->AEGP_GetMatchName(fxRef, match);
                            if (!strcmp(match, MINC_MATCH_NAME)) {
                                ++seen;
                                AEGP_MemHandle nameH = nullptr;
                                char name8[512] = "";
                                if (sts->AEGP_GetStreamName(g_aegpID, fxRef, FALSE, &nameH) == A_Err_NONE)
                                    Utf16HandleToUtf8(suites, nameH, name8, sizeof(name8));
                                MinColorArb want;
                                if (ParseGrammar(name8, &want)) {
                                    /* write via the EFFECT's param stream (Projector-sample pattern):
                                       parade child index == effect index, verified by matchname */
                                    AEGP_EffectRefH effH = nullptr;
                                    if (efs->AEGP_GetLayerEffectByIndex(g_aegpID, layerH, fi, &effH) == A_Err_NONE && effH) {
                                        AEGP_InstalledEffectKey key = 0;
                                        A_char em[64] = "";
                                        efs->AEGP_GetInstalledKeyFromLayerEffect(effH, &key);
                                        efs->AEGP_GetEffectMatchName(key, em);
                                        if (strcmp(em, MINC_MATCH_NAME) != 0) { AuthLog("sync: index misalign '%s'", em); efs->AEGP_DisposeEffect(effH); effH = nullptr; }
                                    }
                                    if (effH) {
                                        /* transport: CallGeneric -> seq data + registry (names are the durable store; auto-sync re-derives on project open) */
                                        MincSyncPayload pay; memset(&pay, 0, sizeof(pay));
                                        pay.magic = MINC_ARB_MAGIC; pay.arb = want;
                                        pay.payVersion = 2;
                                        if (passBase[0]) {           /* refresh when healthy; empty = receiver keeps its passport */
                                            strncpy(pay.configBase, passBase, sizeof(pay.configBase) - 1);
                                            strncpy(pay.passportWorking, snap.workingSpace, sizeof(pay.passportWorking) - 1);
                                        }
                                        A_Time tg = {0, 100};
                                        if (efs->AEGP_EffectCallGeneric(g_aegpID, effH, &tg, PF_Cmd_COMPLETELY_GENERAL, &pay) == A_Err_NONE) ++wrote;
                                        efs->AEGP_DisposeEffect(effH);
                                    }
                                } else if (name8[0]) { ++badname; AuthLog("sync: unparsed name '%s'", name8); }
                            }
                            sts->AEGP_DisposeStream(fxRef);
                        }
                        sts->AEGP_DisposeStream(fxGroup);
                    }
                    sts->AEGP_DisposeStream(layerRef);
                }
            }
        }
        AEGP_ItemH nextH = nullptr;
        its->AEGP_GetNextProjItem(projH, itemH, &nextH);
        itemH = nextH;
    }
    AuthLog("sync: instances=%d wrote=%d unparsed=%d", seen, wrote, badname);
}

static A_Err CommandHook(AEGP_GlobalRefcon, AEGP_CommandRefcon, AEGP_Command command,
                         AEGP_HookPriority, A_Boolean, A_Boolean *handledPB) {
    if (command == g_syncCmd && g_pica) {
        PF_InData fake = {}; fake.pica_basicP = g_pica;
        MincAuthorityRefresh(&fake);                 /* refresh FIRST — the sync payload now carries
                                                        authority state (idle hook already does this order) */
        SyncFromNames(g_pica);
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

void MincAuthorityGlobalSetup(PF_InData *in_data) {
    if (g_registered) return;
#ifdef AE_OS_WIN
    g_mainTid = GetCurrentThreadId();                   /* GLOBAL_SETUP runs on the main thread */
#endif
    AuthLog("boot: GlobalSetup enter @%.0fms", NowMs());
    try {
        g_pica = in_data->pica_basicP;
        AEGP_SuiteHandler suites(in_data->pica_basicP);
        A_Err e = suites.UtilitySuite6()->AEGP_RegisterWithAEGP(NULL, "minColorCST", &g_aegpID);
        g_registered = (e == A_Err_NONE);
        AuthLog("RegisterWithAEGP err=%d id=%d", (int)e, (int)g_aegpID);
        if (g_registered) {
            A_Err e2 = suites.RegisterSuite5()->AEGP_RegisterIdleHook(g_aegpID, IdleHook, NULL);
            g_hooksOK = (e2 == A_Err_NONE);
            AuthLog("RegisterIdleHook err=%d", (int)e2);   /* M1: single binary confirmed */
            {   /* menu command: the panel's transport for writing arb params */
                Acq<AEGP_CommandSuite1> cs(in_data->pica_basicP, kAEGPCommandSuite, kAEGPCommandSuiteVersion1);
                if (cs && cs->AEGP_GetUniqueCommand(&g_syncCmd) == A_Err_NONE && g_syncCmd) {
                    cs->AEGP_InsertMenuCommand(g_syncCmd, "minColor: Sync From Names", AEGP_Menu_EDIT, AEGP_MENU_INSERT_AT_BOTTOM);
                    A_Err e3 = suites.RegisterSuite5()->AEGP_RegisterCommandHook(g_aegpID, AEGP_HP_BeforeAE, g_syncCmd, CommandHook, NULL);
                    A_Err e4 = suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(g_aegpID, UpdateMenuHook, NULL);
                    AuthLog("sync command=%d hook=%d menuhook=%d", (int)g_syncCmd, (int)e3, (int)e4);
                }
            }
        }
    } catch (...) { AuthLog("GlobalSetup: exception"); }
    AuthLog("boot: GlobalSetup exit @%.0fms", NowMs());
    /* no authority refresh here: the idle hook does it within ~1s, and suite calls during the
       startup plugin scan are a boot-time cost we do not need to pay */
}

void MincAuthorityRefresh(PF_InData *in_data) {
#ifdef AE_OS_WIN
    if (GetCurrentThreadId() != g_mainTid) return;      /* AEGP suites: main thread only */
#else
    if (!pthread_main_np()) return;                     /* AEGP suites: main thread only */
#endif
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
