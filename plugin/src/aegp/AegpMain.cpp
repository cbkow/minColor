/* minColorAEGP — the ceremonies bundle. Kind{AEGP}/AEgx, lives in the APP's Plug-ins folder,
   loads at AE LAUNCH (probes D/F, RESULTS §30b/c). M1: ceremonies register here as a command
   TABLE — added incrementally per plan step so the handshake file's commands[] never lies.
   The 0.9.2 panel keeps working against "minColor: Sync From Names" unchanged.             */
#include "MincCore.h"
#include "../ceremony/MincSettings.h"
#include "../ceremony/MincEffectOps.h"
#include "../ceremony/MincDoctor.h"
#include "../ceremony/MincInterpret.h"
#include "../ceremony/MincCeremonyProject.h"
#include "../ceremony/MincPicker.h"
#include "../ceremony/MincStrip.h"
#include "../ceremony/MincArchive.h"
#include "../ceremony/MincMenusWrite.h"
#include "../ceremony/MincArgs.h"
#include "../ceremony/MincUtility.h"
#include <cstdio>
#include <vector>

static AEGP_PluginID  g_id = 0;
static SPBasicSuite  *g_pica = nullptr;

/* ---------------- command table ---------------- */
typedef void (*MincCmdHandler)(void);
struct MincCommandDef {
    const char    *label;
    MincCmdHandler handler;
    AEGP_Command   cmd;
    bool           doctorAfter;   /* refresh reports/doctor-last.json once the ceremony has run */
};

static void CmdSync(void);
static void CmdAbout(void);
static void CmdDoctor(void);
static void CmdInterpret(void);
static void CmdInterpretSel(void);
static void CmdUtility(void);
static void CmdApplyLook(void);
static void CmdRepair(void);
static void CmdRenderPreset(void);
static void CmdMigrate(void);
static void CmdStripForeign(void);
static void CmdStripAll(void);

static MincCommandDef g_commands[] = {
    { "minColor: Sync From Names",     CmdSync,         0, true  },
    { "minColor: About",               CmdAbout,        0, false },
    { "minColor: Doctor",              CmdDoctor,       0, false },   /* writes its own report */
    { "minColor: Interpret Timeline",  CmdInterpret,    0, true  },
    { "minColor: Interpret Selected",  CmdInterpretSel, 0, true  },
    { "minColor: Utility Layers",      CmdUtility,      0, true  },
    { "minColor: Apply Look",          CmdApplyLook,    0, true  },
    { "minColor: Repair",              CmdRepair,       0, true  },
    { "minColor: Apply Render Preset", CmdRenderPreset, 0, true  },
    { "minColor: Migrate Project",     CmdMigrate,      0, true  },
    { "minColor: Strip Foreign OCIO",  CmdStripForeign, 0, true  },
    { "minColor: Strip ALL",           CmdStripAll,     0, true  },
};
static const int g_nCommands = (int)(sizeof(g_commands) / sizeof(g_commands[0]));

/* Force the active comp to re-render its current frame. Our effects read authority (working
   space/config) + arb at render time, all invisible to AE's seq-data fingerprint, so an
   authority or arb change made from the AEGP leaves the cached frame until the user scrubs.
   PF_TouchActiveItem marks the active item dirty — O(1), NO comp walk — and GuidMix keys the
   new cache. Effect-side the badge does this too (Ui.cpp); this is the AEGP path for the walk.
   PF_TouchActiveItem takes no args (app-global active item), so it's callable via the basic
   suite from an AEGP. (2026-09-03) */
static void MincTouchActiveItem(void) {
    if (!g_pica) return;
    const void *sv = nullptr;
    if (g_pica->AcquireSuite(kPFAdvItemSuite, kPFAdvItemSuiteVersion1, &sv) == kSPNoError && sv) {
        ((const PF_AdvItemSuite1 *)sv)->PF_TouchActiveItem();
        g_pica->ReleaseSuite(kPFAdvItemSuite, kPFAdvItemSuiteVersion1);
    }
}

/* ---------------- handlers ---------------- */
static void CmdSync(void) {
    MincAuthorityRefreshBp(g_pica, g_id);               /* refresh FIRST — the sync payload carries
                                                           authority state (CommandHook already did; cheap) */
    MincSyncFromNames(g_pica, g_id, true);              /* panel/user-invoked sync christens by intent */
    MincTouchActiveItem();                              /* names/arbs changed -> re-render */
}

static void CmdAbout(void) {
    MincArgsConsume("minColor: About");
    char txt[512];
    snprintf(txt, sizeof(txt),
             "minColor %s\nBuild %s\nEffect: MediaCore \xc2\xb7 Ceremonies: this AEGP\n"
             "API handshake: settings/aegp-api.json",
             MINC_VERSION_STR, MINC_BUILD_STAMP);
    MincWriteReport("about", std::string("{ \"version\": \"") + MINC_VERSION_STR +
                             "\", \"buildStamp\": \"" + MINC_BUILD_STAMP + "\" }\n");
    if (MincQuietMode() || MincArgsTakeSilent()) return;                         /* automation seam: no dialogs */
    AEGP_SuiteHandler suites(g_pica);
    A_UTF16Char u16[512];
    MincU8ToU16(txt, u16, 512);
    suites.UtilitySuite6()->AEGP_ReportInfoUnicode(g_id, u16);
}

/* The panel's dropdowns come from plugin-menus.json, which only ceremonies wrote. On a fresh machine
   a project whose pin already resolves is green with no ceremony ever run — so the file never
   appeared and the dropdowns stayed empty (found on a new install, 2026-09-06). Now the menus
   follow the DOCTOR: whenever it identifies a preset and the file is missing or names another
   preset, write it. Idempotent, so a matching file costs one small JSON read. */
static void MenusFollowDoctor(const MincDoctorResult &d) {
    if (d.preset.empty() || MincMenusFileIsFor(d.preset)) return;
    MincLog("menus: doctor identified '%s' but plugin-menus.json is missing/other - writing", d.preset.c_str());
    MincWriteMenusForPreset(d.preset);
}

static void CmdDoctor(void) {
    MincArgsConsume("minColor: Doctor");
    MincAuthorityRefreshBp(g_pica, g_id);
    MincDoctorResult d = MincDoctorDiagnose(g_pica, g_id);
    MincWriteReport("doctor", d.toJson());
    MenusFollowDoctor(d);
    MincLog("doctor: %s — %s", d.status.c_str(), d.text.c_str());
    if (MincQuietMode() || MincArgsTakeSilent()) return;
    std::string msg = d.status + " \xe2\x80\x94 " + d.text;
    AEGP_SuiteHandler suites(g_pica);
    A_UTF16Char u16[600];
    MincU8ToU16(msg.c_str(), u16, 600);
    suites.UtilitySuite6()->AEGP_ReportInfoUnicode(g_id, u16);
}

static void CmdInterpret(void) {
    MincArgsConsume("minColor: Interpret Timeline");
    MincInterpretReport r = MincInterpretTimeline(g_pica, g_id);
    MincTouchActiveItem();                               /* new/changed CSTs -> re-render now */
    MincWriteReport("interpret", r.toJson());
    MincLog("interpret: added=%d skipped=%d flagged=%d failed=%d identity=%d contained=%d%s%s",
            (int)r.added.size(), (int)r.skipped.size(), (int)r.flagged.size(),
            (int)r.failed.size(), (int)r.identity.size(), (int)r.contained.size(),
            r.error.empty() ? "" : " error=", r.error.c_str());
    if (MincQuietMode() || MincArgsTakeSilent() || r.error.empty()) return;   /* success: silent (log+report) */
    char msg[256];
    snprintf(msg, sizeof(msg), "minColor Interpret Timeline: %s", r.error.c_str());
    AEGP_SuiteHandler suites(g_pica);
    A_UTF16Char u16[300];
    MincU8ToU16(msg, u16, 300);
    suites.UtilitySuite6()->AEGP_ReportInfoUnicode(g_id, u16);
}

static void CmdInterpretSel(void) {
    std::string space;
    {   MincJsonPtr a = MincArgsConsume("minColor: Interpret Selected");
        if (a) space = a->str("space");
    }
    MincInterpretReport r = MincInterpretSelection(g_pica, g_id, space);
    MincTouchActiveItem();                               /* new/changed CSTs -> re-render now */
    MincWriteReport("interpret-selected", r.toJson());
    MincLog("interpret-selected: added=%d skipped=%d flagged=%d failed=%d%s%s",
            (int)r.added.size(), (int)r.skipped.size(), (int)r.flagged.size(), (int)r.failed.size(),
            r.error.empty() ? "" : " error=", r.error.c_str());
    if (MincQuietMode() || MincArgsTakeSilent() || r.error.empty()) return;   /* success: silent (log+report) */
    char msg[256];
    snprintf(msg, sizeof(msg), "minColor Interpret Selected: %s", r.error.c_str());
    AEGP_SuiteHandler suites(g_pica);
    A_UTF16Char u16[300];
    MincU8ToU16(msg, u16, 300);
    suites.UtilitySuite6()->AEGP_ReportInfoUnicode(g_id, u16);
}

static void ReportCeremony(const char *name, const std::string &json) {
    MincWriteReport(name, json);
    MincLog("%s: %s", name, json.substr(0, 200).c_str());
    /* success is silent (the change is visible + logged + in reports/<name>-last.json);
       only a FAILURE surfaces, and it shows the REASON (actionable) — not a detail dump. */
    size_t ei = json.find("\"error\"");
    if (MincQuietMode() || MincArgsTakeSilent() || ei == std::string::npos) return;
    std::string reason;                                  /* pull the "error":"..." value */
    size_t q1 = json.find('"', json.find(':', ei) + 1);
    if (q1 != std::string::npos) { size_t q2 = json.find('"', q1 + 1); if (q2 != std::string::npos) reason = json.substr(q1 + 1, q2 - q1 - 1); }
    std::string msg = std::string("minColor ") + name + " failed" + (reason.empty() ? "" : ": " + reason);
    AEGP_SuiteHandler suites(g_pica);
    A_UTF16Char u16[300];
    MincU8ToU16(msg.c_str(), u16, 300);
    suites.UtilitySuite6()->AEGP_ReportInfoUnicode(g_id, u16);
}

static void CmdMigrate(void) {
    std::string key;
    if (!MincPickPreset("minColor: Migrate Project", &key)) { MincLog("migrate: no preset picked"); return; }
    ReportCeremony("migrate", MincMigrateProject(g_pica, g_id, key));
}

static void CmdStripForeign(void) { MincArgsConsume("minColor: Strip Foreign OCIO"); ReportCeremony("strip-foreign", MincStripForeignOcio(g_pica, g_id, false)); }
static void CmdStripAll(void)     { MincArgsConsume("minColor: Strip ALL"); ReportCeremony("strip-all",     MincStripForeignOcio(g_pica, g_id, true)); }
static void CmdRepair(void)       { MincArgsConsume("minColor: Repair"); ReportCeremony("repair",        MincRepairProject(g_pica, g_id)); }
static void CmdRenderPreset(void) {
    std::string name;
    {   MincJsonPtr a = MincArgsConsume("minColor: Apply Render Preset");
        if (a) name = a->str("name");
    }
    ReportCeremony("render-preset", MincApplyRenderPreset(g_pica, g_id, name));
}
static void CmdApplyLook(void) {
    std::string look;
    {   MincJsonPtr a = MincArgsConsume("minColor: Apply Look");
        if (a) look = a->str("look");                 /* "" = remove */
    }
    ReportCeremony("apply-look", MincApplyLook(g_pica, g_id, look));
}
static void CmdUtility(void) {
    std::string view, render;
    {   MincJsonPtr a = MincArgsConsume("minColor: Utility Layers");
        if (a) { view = a->str("view"); render = a->str("render"); }
    }
    ReportCeremony("utility-layers", MincEnsureUtilityLayers(g_pica, g_id, view, render));
}

/* ---------------- hooks ---------------- */
/* NO idle hook (2026-09-06). The AEGP used to diagnose the doctor every ~5 s on idle and
   write doctor-last.json on change; the panel only ever read that file at open, on a lamp
   click and after its own commands — the loop existed to seed one lamp. It also re-parsed
   the saved .aep from disk every tick. Now the report is written by the Doctor command and
   after every ceremony (doctorAfter), and the panel checks the report's projPath against
   the open project before trusting it. The effect binary keeps its own refresh-only idle
   hook (EffectAuthority.cpp) — render never depended on this one. */

static void DoctorAfterCommand(const char *label) {
    MincAuthorityRefreshBp(g_pica, g_id);
    MincDoctorResult d = MincDoctorDiagnose(g_pica, g_id);
    MincWriteReport("doctor", d.toJson());
    MenusFollowDoctor(d);
    MincLog("doctor (after '%s'): %s", label, d.status.c_str());
}

static A_Err CommandHook(AEGP_GlobalRefcon, AEGP_CommandRefcon, AEGP_Command command,
                         AEGP_HookPriority, A_Boolean, A_Boolean *handledPB) {
    if (!g_pica) return A_Err_NONE;
    for (int i = 0; i < g_nCommands; ++i) {
        if (g_commands[i].cmd && command == g_commands[i].cmd) {
            MincInstalledKey(g_pica, g_id);      /* timed scan, once per session (RESULTS §31) — lazy, first command */
            MincAuthorityRefreshBp(g_pica, g_id);/* every ceremony starts from a fresh authority snapshot */
            MincArgsResetSilent();               /* silent never leaks across dispatches */
            try { g_commands[i].handler(); }
            catch (...) { MincLog("command '%s': exception", g_commands[i].label); }
            if (g_commands[i].doctorAfter) {
                try { DoctorAfterCommand(g_commands[i].label); }
                catch (...) { MincLog("doctor after '%s': exception", g_commands[i].label); }
            }
            if (handledPB) *handledPB = TRUE;
            break;
        }
    }
    return A_Err_NONE;
}

static A_Err UpdateMenuHook(AEGP_GlobalRefcon, AEGP_UpdateMenuRefcon, AEGP_WindowType) {
    if (!g_pica) return A_Err_NONE;
    Acq<AEGP_CommandSuite1> cs(g_pica, kAEGPCommandSuite, kAEGPCommandSuiteVersion1);
    if (cs) for (int i = 0; i < g_nCommands; ++i)
        if (g_commands[i].cmd) cs->AEGP_EnableCommand(g_commands[i].cmd);
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
        Acq<AEGP_CommandSuite1> cs(pica_basicP, kAEGPCommandSuite, kAEGPCommandSuiteVersion1);
        int registered = 0;
        if (cs) {
            for (int i = 0; i < g_nCommands; ++i) {
                if (cs->AEGP_GetUniqueCommand(&g_commands[i].cmd) != A_Err_NONE || !g_commands[i].cmd) continue;
                cs->AEGP_InsertMenuCommand(g_commands[i].cmd, g_commands[i].label, AEGP_Menu_EDIT, AEGP_MENU_INSERT_AT_BOTTOM);
                ++registered;
            }
            A_Err e3 = suites.RegisterSuite5()->AEGP_RegisterCommandHook(g_id, AEGP_HP_BeforeAE, AEGP_Command_ALL, CommandHook, NULL);
            A_Err e4 = suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(g_id, UpdateMenuHook, NULL);
            MincLog("commands registered=%d hook=%d menuhook=%d", registered, (int)e3, (int)e4);
        }
        if (registered > 0) {
            std::vector<const char *> labels;            /* sized to the table — never a fixed cap
                                                            (a 17th command overran labels[16] and
                                                            crashed at launch, 2026-09-03) */
            for (int i = 0; i < g_nCommands; ++i) labels.push_back(g_commands[i].label);
            MincWriteHandshake(labels.data(), g_nCommands);   /* AFTER registration — never lies */
            MincSeedSettings();   /* write the shell's disk JSON from embedded — installer ships none */
        } else { MincLog("command registration FAILED"); out = A_Err_GENERIC; }
    } catch (...) { MincLog("AegpEntry: exception"); out = A_Err_GENERIC; }
    return out;
}
