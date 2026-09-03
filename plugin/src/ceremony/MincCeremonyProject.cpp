#include "MincCeremonyProject.h"
#include "MincPresets.h"
#include "MincSettings.h"
#include "MincSuggest.h"
#include "MincEffectOps.h"
#include "MincTranslate.h"
#include "MincArchive.h"
#include "MincMenusWrite.h"
#include "MincDoctor.h"
#include "MincJson.h"
#include "../core/MincRifx.h"
#include <cstdio>
#include <ctime>
#include <map>
#include <set>
#include <vector>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

/* ---------------- small utilities ---------------- */
static std::string JStr(const std::string &s) {
    std::string o = "\"";
    for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
    return o + "\"";
}
static std::string JArr(const std::vector<std::string> &v) {
    std::string o = "[";
    for (size_t i = 0; i < v.size(); ++i) { if (i) o += ", "; o += JStr(v[i]); }
    return o + "]";
}
static bool PathExists(const std::string &p) { struct stat st; return !p.empty() && stat(p.c_str(), &st) == 0; }
static std::string Dirname(const std::string &p) {
    size_t s = p.find_last_of('/');
    return s == std::string::npos ? std::string() : p.substr(0, s);
}
static std::string Basename2(const std::string &p) {
    size_t s = p.find_last_of('/');
    return s == std::string::npos ? p : p.substr(s + 1);
}
static bool CopyFileBytes(const std::string &from, const std::string &to) {
    FILE *a = fopen(from.c_str(), "rb");
    if (!a) return false;
    FILE *b = fopen(to.c_str(), "wb");
    if (!b) { fclose(a); return false; }
    char buf[65536]; size_t n; bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), a)) > 0) if (fwrite(buf, 1, n, b) != n) { ok = false; break; }
    fclose(a); fclose(b);
    return ok;
}

/* ---------------- AE-side helpers ---------------- */
struct PEnv {
    SPBasicSuite *bp; AEGP_PluginID id;
    AEGP_SuiteHandler *suites;
    AEGP_ProjSuite6 *pjs; AEGP_ItemSuite9 *its; AEGP_CompSuite12 *cps;
    AEGP_LayerSuite9 *lys; AEGP_FootageSuite5 *fts; AEGP_UtilitySuite6 *uts;
};
static std::string ProjPath(PEnv &e) {
    AEGP_ProjectH projH = nullptr;
    e.pjs->AEGP_GetProjectByIndex(0, &projH);
    if (!projH) return "";
    AEGP_MemHandle ph = nullptr;
    char b[2048] = "";
    if (e.pjs->AEGP_GetProjectPath(projH, &ph) == A_Err_NONE)
        MincUtf16HandleToUtf8(*e.suites, ph, b, sizeof(b));
    return b;
}
static bool SaveTo(PEnv &e, const std::string &path) {
    AEGP_ProjectH projH = nullptr;
    e.pjs->AEGP_GetProjectByIndex(0, &projH);
    if (!projH) return false;
    A_UTF16Char u16[1024];
    MincU8ToU16(path.c_str(), u16, 1024);
    return e.pjs->AEGP_SaveProjectToPath(projH, u16) == A_Err_NONE;
}
static bool Reopen(PEnv &e, const std::string &path) {
    A_UTF16Char u16[1024];
    MincU8ToU16(path.c_str(), u16, 1024);
    AEGP_ErrReportState errState;                         /* OUT struct — nullptr here CRASHED AE */
    if (e.uts) e.uts->AEGP_StartQuietErrors(&errState);   /* missing-footage etc. on reopen */
    AEGP_ProjectH nh = nullptr;
    A_Err err = e.pjs->AEGP_OpenProjectFromPath(u16, &nh);
    if (e.uts) e.uts->AEGP_EndQuietErrors(FALSE, &errState);
    return err == A_Err_NONE;
}
static std::string BackupCopy(const std::string &projPath, const char *tag, std::string *err) {
    std::string dir = Dirname(projPath) + "/_minColor";
    mkdir(dir.c_str(), 0777);
    dir += "/backups";
    mkdir(dir.c_str(), 0777);
    time_t t = time(nullptr);
    struct tm tmv; localtime_r(&t, &tmv);
    char stamp[32];
    snprintf(stamp, sizeof(stamp), "%04d%02d%02d-%02d%02d%02d",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    std::string name = Basename2(projPath);
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".aep") == 0) name = name.substr(0, name.size() - 4);
    std::string base = dir + "/" + name + "_" + tag + "_" + stamp;
    std::string bpath = base + ".aep";
    int n = 2;
    while (PathExists(bpath)) { char sfx[16]; snprintf(sfx, sizeof(sfx), "-%d", n++); bpath = base + sfx + ".aep"; }
    if (!CopyFileBytes(projPath, bpath)) { if (err) *err = "backup copy failed"; return ""; }
    return bpath;
}
static std::string ProvenanceRecord(const std::string &preset, const std::string &config) {
    time_t t = time(nullptr);
    struct tm tmv; localtime_r(&t, &tmv);
    char date[16];
    snprintf(date, sizeof(date), "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
    return std::string("{\"tool\":\"minColor\",\"panel\":\"") + MINC_VERSION_STR +
           "\",\"plugin\":\"" + MINC_VERSION_STR + "\",\"ocioEngine\":\"native\",\"preset\":\"" + preset +
           "\",\"config\":\"" + config + "\",\"date\":\"" + date + "\"}";
}
static void SetBitDepth(PEnv &e, const std::string &family, int *out) {
    AEGP_ProjectH projH = nullptr;
    e.pjs->AEGP_GetProjectByIndex(0, &projH);
    AEGP_ProjBitDepth want = (family == "Display") ? AEGP_ProjBitDepth_16 : AEGP_ProjBitDepth_32;
    if (projH) e.pjs->AEGP_SetProjectBitDepth(projH, want);
    *out = (family == "Display") ? 16 : 32;
}
static bool AcquireEnv(SPBasicSuite *bp, AEGP_PluginID id, AEGP_SuiteHandler &suites, PEnv &e,
                       Acq<AEGP_ProjSuite6> &pjs, Acq<AEGP_ItemSuite9> &its, Acq<AEGP_CompSuite12> &cps,
                       Acq<AEGP_LayerSuite9> &lys, Acq<AEGP_FootageSuite5> &fts, Acq<AEGP_UtilitySuite6> &uts) {
    if (!pjs || !its || !cps || !lys || !fts) return false;
    e.bp = bp; e.id = id; e.suites = &suites;
    e.pjs = pjs.p; e.its = its.p; e.cps = cps.p; e.lys = lys.p; e.fts = fts.p; e.uts = uts.p;
    return true;
}

/* ---------------- syncReport rows (port :683-701; only okFlag/current/id/name needed) ---------------- */
struct SyncRow { int32_t id; std::string item, current; bool ok; };
static bool BuildSyncRows(PEnv &e, const std::string &projPath, std::vector<SyncRow> *rows, std::string *workName) {
    /* temp-copy read: identical data to the panel's forced save, user's file untouched */
    std::string tmp = "/tmp/minColor-migrate-scan.aep";
    if (!SaveTo(e, tmp)) return false;
    MincAssignments a;
    bool ok = MincRifxReadAssignments(tmp.c_str(), &a);
    unlink(tmp.c_str());
    if (!ok) return false;
    std::map<int32_t, const MincAssignItem *> byId;
    for (auto &it2 : a.items) if (it2.id) byId[it2.id] = &it2;
    std::string wn = a.working.empty() ? "?" : a.working.substr(a.working.find('/') == std::string::npos ? 0 : a.working.find('/') + 1);
    *workName = wn;
    AEGP_ProjectH projH = nullptr;
    e.pjs->AEGP_GetProjectByIndex(0, &projH);
    AEGP_ItemH itH = nullptr;
    e.its->AEGP_GetFirstProjItem(projH, &itH);
    while (itH) {
        AEGP_ItemType ty = AEGP_ItemType_NONE;
        e.its->AEGP_GetItemType(itH, &ty);
        if (ty == AEGP_ItemType_FOOTAGE) {
            AEGP_FootageH ftg = nullptr;
            char fpath[2048] = "";
            if (e.fts->AEGP_GetMainFootageFromItem(itH, &ftg) == A_Err_NONE && ftg) {
                AEGP_MemHandle ph = nullptr;
                if (e.fts->AEGP_GetFootagePath(ftg, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, &ph) == A_Err_NONE)
                    MincUtf16HandleToUtf8(*e.suites, ph, fpath, sizeof(fpath));
            }
            if (fpath[0]) {                              /* FileSource-with-file filter (:693) */
                SyncRow r;
                A_long iid = 0; e.its->AEGP_GetItemID(itH, &iid);
                r.id = (int32_t)iid;
                AEGP_MemHandle nh = nullptr;
                char nb[512] = "";
                if (e.its->AEGP_GetItemName(e.id, itH, &nh) == A_Err_NONE)
                    MincUtf16HandleToUtf8(*e.suites, nh, nb, sizeof(nb));
                r.item = nb;
                auto f = byId.find(r.id);
                if (f != byId.end()) {
                    r.current = f->second->colorspace + (f->second->view.empty() ? "" : " (view: " + f->second->view + ")");
                    std::string curName = f->second->colorspace.substr(f->second->colorspace.find('/') == std::string::npos ? 0 : f->second->colorspace.find('/') + 1);
                    r.ok = (curName == wn || curName == "default");
                } else { r.current = "(none)"; r.ok = true; }
                rows->push_back(r);
            }
        }
        AEGP_ItemH nx = nullptr;
        e.its->AEGP_GetNextProjItem(projH, itH, &nx);
        itH = nx;
    }
    return true;
}

/* ---------------- rebuild walk (port rebuildMinColorEffects :369-488) ---------------- */
struct RebuildOut {
    std::vector<std::string> rebuilt, failed, foreign, orphans, strippedPipeline, gradesLeft, remapped, removed;
    int viewRender = 0;
};
static bool ConfigHasLooks(const std::string &pin) {      /* configLooks() truthiness (:1241) */
    std::string s = MincReadTextFile(pin);
    size_t li = s.find("\nlooks:");
    if (li == std::string::npos) return false;
    size_t end = s.find("\nc", li + 7);                   /* next top-level key approximation; name scan below decides */
    std::string sect = s.substr(li + 7, 4000);
    return sect.find("name:") != std::string::npos;
}
static void RebuildEffects(PEnv &e, const MincSuggestCtx &ctx, const std::set<std::string> &configLooks,
                           RebuildOut *out, bool *touchedMinc) {
    const bool hasLooks = !configLooks.empty();
    AEGP_ProjectH projH = nullptr;
    e.pjs->AEGP_GetProjectByIndex(0, &projH);
    AEGP_ItemH itemH = nullptr;
    e.its->AEGP_GetFirstProjItem(projH, &itemH);
    while (itemH) {
        AEGP_ItemType ty = AEGP_ItemType_NONE;
        e.its->AEGP_GetItemType(itemH, &ty);
        if (ty == AEGP_ItemType_COMP) {
            AEGP_CompH compH = nullptr;
            if (e.cps->AEGP_GetCompFromItem(itemH, &compH) == A_Err_NONE && compH) {
                AEGP_MemHandle inh = nullptr; char compName[512] = "";
                if (e.its->AEGP_GetItemName(e.id, itemH, &inh) == A_Err_NONE)
                    MincUtf16HandleToUtf8(*e.suites, inh, compName, sizeof(compName));
                A_long nL = 0;
                e.lys->AEGP_GetCompNumLayers(compH, &nL);
                for (A_long li = 0; li < nL; ++li) {
                    AEGP_LayerH ly = nullptr;
                    if (e.lys->AEGP_GetCompLayerByIndex(compH, li, &ly) != A_Err_NONE || !ly) continue;
                    AEGP_MemHandle lnh = nullptr, lsh = nullptr; char lyName[512] = "";
                    if (e.lys->AEGP_GetLayerName(e.id, ly, &lnh, &lsh) == A_Err_NONE) {
                        MincUtf16HandleToUtf8(*e.suites, lnh, lyName, sizeof(lyName));
                        if (!lyName[0] && lsh) MincUtf16HandleToUtf8(*e.suites, lsh, lyName, sizeof(lyName));
                        else if (lsh) e.suites->MemorySuite1()->AEGP_FreeMemHandle(lsh);
                    }
                    std::string label = std::string(compName) + "/" + lyName;
                    std::vector<MincFxEntry> fx;
                    MincEnumLayerEffects(e.bp, e.id, ly, &fx);
                    {   /* orphan utility hosts (:383-387) */
                        AEGP_LayerFlags fl = 0;
                        e.lys->AEGP_GetLayerFlags(ly, &fl);
                        std::string ln = lyName;
                        if ((fl & AEGP_LayerFlag_ADJUSTMENT_LAYER) &&
                            (ln.compare(0, 14, "minColor VIEW ") == 0 || ln == "minColor VIEW" || ln.compare(0, 15, "minColor RENDER") == 0)) {
                            bool hasOurs = false;
                            for (auto &f2 : fx) if (f2.name.compare(0, 10, "minColor: ") == 0) { hasOurs = true; break; }
                            if (!hasOurs) out->orphans.push_back(label);
                        }
                    }
                    for (int k = 0; k < (int)fx.size(); ++k) {
                        const std::string &nm = fx[k].name, &mn = fx[k].match;
                        bool oursName = nm.compare(0, 10, "minColor: ") == 0;
                        if (mn == "ADBE OCIO Look Transform" && oursName) {   /* :394-404 */
                            /* A native OCIO Look Transform referencing a look ABSENT from the new
                               config is the crash landmine: AE's OCIO THROWS on a missing look and
                               aborts (RESULTS §41). Check THIS look, not just whether the config has
                               any looks — a preset with other looks but not this one still strands.
                               Our own MINC LOOK effect is immune (it passes through), so keeping a
                               *valid* native look is fine; an invalid one must go.                  */
                            std::string lk = (nm.compare(0, 15, "minColor: look ") == 0) ? nm.substr(15) : "";
                            if (lk.empty() || !configLooks.count(lk)) {
                                if (MincRemoveEffectAt(e.bp, e.id, ly, k + 1)) {
                                    out->removed.push_back(label + " [" + nm + "] \xe2\x80\x94 look not in this preset's config");
                                    fx.erase(fx.begin() + k); --k;
                                } else out->failed.push_back(label + " [" + nm + "] \xe2\x80\x94 remove failed");
                                continue;
                            }
                            out->gradesLeft.push_back(label + " [" + nm + "]");
                            continue;
                        }
                        MincVerb mv = MINC_VERB_LEGACY;
                        if (MincMatchVerb(mn.c_str(), &mv) && oursName) {     /* :405-415 — all five ours */
                            MincFxName pm = MincParseFxName(nm);
                            if (!pm.valid) continue;
                            /* variant whose name-verb contradicts the match name: unparsed — leave, never reinterpret */
                            if (!MincKindMatchesVerb(pm.kind.c_str(), mv)) continue;
                            bool legacy = (mv == MINC_VERB_LEGACY);
                            /* legacy resurrection (M3 step 8): a MINC CST instance is a PLACEHOLDER
                               wherever the legacy effect no longer registers (mac from 2.0) — remove
                               it and re-author the variant that owns its kind, same name grammar,
                               same slot. The post-reopen walk writes the popups from the name.      */
                            auto resurrect = [&](const std::string &kind, const std::string &space) {
                                const std::string nmC = nm;
                                int idxL = k + 1;
                                bool wl = MincLayerLocked(e.bp, ly);
                                if (wl) MincSetLayerLocked(e.bp, ly, false);
                                if (!MincRemoveEffectAt(e.bp, e.id, ly, idxL)) {
                                    out->failed.push_back(label + " \xe2\x80\x94 remove failed");
                                    if (wl) MincSetLayerLocked(e.bp, ly, true);
                                    return;
                                }
                                std::string newName = (kind == "input")
                                    ? "minColor: " + space + " \xe2\x86\x92 working"
                                    : "minColor: " + kind + " " + space;
                                const char *mfk = MincMatchForKind(kind.c_str());
                                int endIdx = 0;
                                AEGP_EffectRefH rfx = MincApplyByMatchWithName(e.bp, e.id, ly, mfk, newName, &endIdx);
                                if (rfx) {
                                    { Acq<AEGP_EffectSuite5> ef5(e.bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5); if (ef5) ef5->AEGP_DisposeEffect(rfx); }
                                    if (endIdx != idxL) MincMoveEffect(e.bp, e.id, ly, endIdx, idxL);
                                    out->rebuilt.push_back(label + " [" + nmC + "] legacy \xe2\x86\x92 variant");
                                    fx[k].match = mfk;
                                    fx[k].name = newName;
                                    *touchedMinc = true;
                                } else {
                                    out->failed.push_back(label + " \xe2\x80\x94 legacy rebuild failed (removed; re-interpret)");
                                    fx.erase(fx.begin() + k); --k;
                                }
                                if (wl) MincSetLayerLocked(e.bp, ly, true);
                            };
                            if (pm.kind == "look") {
                                if (!hasLooks) {
                                    if (MincRemoveEffectAt(e.bp, e.id, ly, k + 1)) {
                                        out->removed.push_back(label + " [" + nm + "] \xe2\x80\x94 looks do not exist in this preset");
                                        fx.erase(fx.begin() + k); --k;
                                    } else out->failed.push_back(label + " \xe2\x80\x94 remove failed");
                                    continue;
                                }
                                if (legacy) resurrect("look", pm.space);      /* looks carry their space as-is */
                                continue;
                            }
                            MincRemap rm = MincRemapSpace(pm.kind, pm.space, ctx);
                            if (rm.space.empty()) {
                                if (MincRemoveEffectAt(e.bp, e.id, ly, k + 1)) {
                                    (rm.identity ? out->remapped : out->failed).push_back(label + " \xe2\x80\x94 " + rm.note + (rm.identity ? "" : " (removed; re-interpret)"));
                                    fx.erase(fx.begin() + k); --k;
                                } else out->failed.push_back(label + " \xe2\x80\x94 remove failed");
                                continue;
                            }
                            if (legacy) {                                     /* changed or not: the placeholder dies */
                                if (rm.changed) out->remapped.push_back(label + ": " + rm.note);
                                resurrect(pm.kind, rm.space);
                                continue;
                            }
                            if (rm.changed) {
                                std::string newName = (pm.kind == "input")
                                    ? "minColor: " + rm.space + " \xe2\x86\x92 working"
                                    : "minColor: " + pm.kind + " " + rm.space;
                                if (MincRenameEffectAt(e.bp, e.id, ly, k + 1, newName)) {
                                    out->remapped.push_back(label + ": " + rm.note);
                                    *touchedMinc = true;
                                    fx[k].name = newName;
                                } else out->failed.push_back(label + " \xe2\x80\x94 rename failed");
                            }
                            continue;
                        }
                        if (mn == "ADBE OCIO Display Transform" || mn == "ADBE OCIO Look Transform" ||
                            (mn == "ADBE OCIO Color Space Transform" && !oursName)) {   /* :417-425 */
                            if (MincRemoveEffectAt(e.bp, e.id, ly, k + 1)) {
                                out->strippedPipeline.push_back(label + " [" + nm + "]");
                                fx.erase(fx.begin() + k); --k;
                            } else out->failed.push_back(label + " [" + nm + "] \xe2\x80\x94 remove failed");
                            continue;
                        }
                        if (mn == "ADBE OCIO CDL Transform" || mn == "ADBE OCIO FILE Transform") {   /* :427-432 */
                            out->gradesLeft.push_back(label + " [" + nm + "]");
                            continue;
                        }
                        /* native minColor-named CSTs (:434-482): retarget IN PLACE on the host
                           layer — M1's strip-and-report deviation closed in M2 step 6. Popups
                           are written BEFORE the reorder (§34: refs are position-bound).    */
                        if (mn == "ADBE OCIO Color Space Transform" && oursName) {
                            const std::string nmC = nm;              /* copies — fx[k] is rewritten below */
                            std::string rest = nmC.substr(10);
                            bool wl = MincLayerLocked(e.bp, ly);
                            if (wl) MincSetLayerLocked(e.bp, ly, false);
                            auto relock = [&] { if (wl) MincSetLayerLocked(e.bp, ly, true); };
                            std::string kind;
                            if      (rest.compare(0, 5, "view ") == 0)   kind = "view";
                            else if (rest.compare(0, 7, "render ") == 0) kind = "render";
                            if (!kind.empty()) {                     /* :440-452 */
                                std::string space = rest.substr(kind.size() + 1);
                                MincRemap rv = MincRemapSpace(kind, space, ctx);  /* always resolves (family default last resort) */
                                if (rv.changed) out->remapped.push_back(label + ": " + rv.note);
                                int idxV = k + 1;
                                if (!MincRemoveEffectAt(e.bp, e.id, ly, idxV)) { out->failed.push_back(label + " \xe2\x80\x94 remove failed"); relock(); continue; }
                                std::string newName = "minColor: " + kind + " " + rv.space;
                                int endIdx = 0;
                                AEGP_EffectRefH vfx = MincApplyByMatchWithName(e.bp, e.id, ly, "ADBE OCIO Color Space Transform", newName, &endIdx);
                                std::string e1, e2;
                                if (vfx && MincSetPopupByName(e.bp, e.id, vfx, 1, "default", &e1) && MincSetPopupByName(e.bp, e.id, vfx, 2, rv.space, &e2)) {
                                    out->viewRender++;
                                    out->rebuilt.push_back(label + " (" + rest + ")");
                                    fx[k].name = newName;            /* replaced in place */
                                } else out->failed.push_back(label + " \xe2\x80\x94 " + (vfx ? (e1.empty() ? e2 : e1) : "native apply failed"));
                                if (vfx) {
                                    { Acq<AEGP_EffectSuite5> ef4(e.bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5); if (ef4) ef4->AEGP_DisposeEffect(vfx); }
                                    if (endIdx != idxV) MincMoveEffect(e.bp, e.id, ly, endIdx, idxV);
                                } else { fx.erase(fx.begin() + k); --k; }
                                relock(); continue;
                            }
                            if (rest.compare(0, 8, "contain ") == 0) {   /* :455-468 */
                                std::string cs = rest.substr(8);
                                MincRemap rc = MincRemapSpace("contain", cs, ctx);
                                if (rc.space.empty()) {
                                    if (MincRemoveEffectAt(e.bp, e.id, ly, k + 1)) {
                                        out->failed.push_back(label + " \xe2\x80\x94 " + rc.note + " (removed; re-interpret)");
                                        fx.erase(fx.begin() + k); --k;
                                    } else out->failed.push_back(label + " \xe2\x80\x94 remove failed");
                                    relock(); continue;
                                }
                                if (rc.changed) out->remapped.push_back(label + ": " + rc.note);
                                int idxC = k + 1;
                                if (!MincRemoveEffectAt(e.bp, e.id, ly, idxC)) { out->failed.push_back(label + " \xe2\x80\x94 remove failed"); relock(); continue; }
                                std::string newName = "minColor: contain " + rc.space;
                                int endIdx = 0;
                                AEGP_EffectRefH cfx = MincApplyByMatchWithName(e.bp, e.id, ly, "ADBE OCIO Color Space Transform", newName, &endIdx);
                                std::string e1, e2;
                                if (cfx && MincSetPopupByName(e.bp, e.id, cfx, 1, rc.space, &e1) && MincSetPopupByName(e.bp, e.id, cfx, 2, "default", &e2)) {
                                    out->rebuilt.push_back(label + " (" + rest + ")");
                                    fx[k].name = newName;
                                } else out->failed.push_back(label + " \xe2\x80\x94 " + (cfx ? (e1.empty() ? e2 : e1) : "native apply failed"));
                                if (cfx) {
                                    { Acq<AEGP_EffectSuite5> ef5(e.bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5); if (ef5) ef5->AEGP_DisposeEffect(cfx); }
                                    if (endIdx != idxC) MincMoveEffect(e.bp, e.id, ly, endIdx, idxC);
                                } else { fx.erase(fx.begin() + k); --k; }
                                relock(); continue;
                            }
                            size_t ai = rest.find(" \xe2\x86\x92 working");    /* :470-478 */
                            if (ai == std::string::npos || ai == 0) { relock(); continue; }   /* left in place, panel parity */
                            std::string space2 = rest.substr(0, ai);
                            MincRemap ri = MincRemapSpace("input", space2, ctx);
                            if (ri.space.empty()) {
                                if (MincRemoveEffectAt(e.bp, e.id, ly, k + 1)) {
                                    (ri.identity ? out->remapped : out->failed).push_back(label + " \xe2\x80\x94 " + ri.note + (ri.identity ? "" : " (removed; re-interpret)"));
                                    fx.erase(fx.begin() + k); --k;
                                } else out->failed.push_back(label + " \xe2\x80\x94 remove failed");
                                relock(); continue;
                            }
                            if (ri.changed) out->remapped.push_back(label + ": " + ri.note);
                            int idxI = k + 1;
                            if (!MincRemoveEffectAt(e.bp, e.id, ly, idxI)) { out->failed.push_back(label + " \xe2\x80\x94 remove failed"); relock(); continue; }
                            std::string mname = "minColor: " + ri.space + " \xe2\x86\x92 working";
                            if (MincApplyMincWithName(e.bp, e.id, ly, mname, idxI)) {   /* rebuilt as the XFORM variant (M3 authoring swap) */
                                out->rebuilt.push_back(label + " (" + rest + ")");
                                fx[k].match = MINC_MATCH_XFORM;
                                fx[k].name = mname;
                                *touchedMinc = true;
                            } else {
                                out->failed.push_back(label + " \xe2\x80\x94 native apply failed");
                                fx.erase(fx.begin() + k); --k;
                            }
                            relock(); continue;
                        }
                    }
                }
            }
        }
        AEGP_ItemH nextH = nullptr;
        e.its->AEGP_GetNextProjItem(projH, itemH, &nextH);
        itemH = nextH;
    }
}

/* ---------------- ceremonies ---------------- */
std::string MincApplyPresetToCurrent(SPBasicSuite *bp, AEGP_PluginID id, const std::string &presetKey) {
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ProjSuite6> pjs(bp, kAEGPProjSuite, kAEGPProjSuiteVersion6);
    Acq<AEGP_ItemSuite9> its(bp, kAEGPItemSuite, kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12> cps(bp, kAEGPCompSuite, kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9> lys(bp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    Acq<AEGP_FootageSuite5> fts(bp, kAEGPFootageSuite, kAEGPFootageSuiteVersion5);
    Acq<AEGP_UtilitySuite6> uts(bp, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    PEnv e;
    if (!AcquireEnv(bp, id, suites, e, pjs, its, cps, lys, fts, uts)) return "{ \"error\": \"suite acquire failed\" }\n";
    MincPresetInfo pr = MincPresetMeta(presetKey);
    if (!pr.valid) return "{ \"error\": " + JStr("unknown preset " + presetKey) + " }\n";
    std::string projPath = ProjPath(e);
    if (projPath.empty()) return "{ \"error\": \"save the project first\" }\n";
    std::string cfg = MincCentralConfigsDir() + "/" + pr.config;
    if (!PathExists(cfg)) return "{ \"error\": " + JStr("config not in central store: " + pr.config) + " }\n";
    if (!SaveTo(e, projPath)) return "{ \"error\": \"save failed\" }\n";
    std::string berr, bpath = BackupCopy(projPath, "prepatch", &berr);
    if (bpath.empty()) return "{ \"error\": " + JStr(berr) + " }\n";
    std::vector<MincFootagePatch> none;
    std::vector<MincXmpUpsert> xmp = { { "provenance", ProvenanceRecord(presetKey, pr.config) } };
    std::string perr;
    if (!MincRifxPatchProject(projPath.c_str(), cfg.c_str(), pr.pwcsJSON.c_str(), none, xmp, &perr))
        return "{ \"error\": " + JStr("patch failed: " + perr) + " }\n";
    if (!Reopen(e, projPath)) return "{ \"error\": \"reopen failed\" }\n";
    int bpc = 0;
    SetBitDepth(e, pr.family, &bpc);                     /* family depth (Linear=32, Display=16) — the
                                                            reopened project otherwise keeps AE's sticky
                                                            depth (a lin2020 Set Up once landed at 16,
                                                            found live 2026-09-02; Migrate always did this) */
    MincAuthorityRefreshBp(bp, id);
    MincWriteMenus(bp, id);                              /* menus follow the new pin immediately */
    char bpcS[16];
    snprintf(bpcS, sizeof(bpcS), "%d", bpc);
    return "{ \"working\": " + JStr(pr.workingSpaceLabel) + ", \"bitsPerChannel\": " + bpcS +
           ", \"backup\": " + JStr(bpath) + " }\n";
}

std::string MincMigrateProject(SPBasicSuite *bp, AEGP_PluginID id, const std::string &presetKey) {
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ProjSuite6> pjs(bp, kAEGPProjSuite, kAEGPProjSuiteVersion6);
    Acq<AEGP_ItemSuite9> its(bp, kAEGPItemSuite, kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12> cps(bp, kAEGPCompSuite, kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9> lys(bp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    Acq<AEGP_FootageSuite5> fts(bp, kAEGPFootageSuite, kAEGPFootageSuiteVersion5);
    Acq<AEGP_UtilitySuite6> uts(bp, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    PEnv e;
    if (!AcquireEnv(bp, id, suites, e, pjs, its, cps, lys, fts, uts)) return "{ \"error\": \"suite acquire failed\" }\n";
    MincPresetInfo pr = MincPresetMeta(presetKey);
    if (!pr.valid) return "{ \"error\": " + JStr("unknown preset " + presetKey) + " }\n";
    std::string projPath = ProjPath(e);
    if (projPath.empty()) return "{ \"error\": \"save the project first\" }\n";
    std::string cfg = MincCentralConfigsDir() + "/" + pr.config;
    if (!PathExists(cfg)) return "{ \"error\": " + JStr("config not in central store: " + pr.config) + " }\n";

    /* pre-reopen half */
    std::vector<SyncRow> rows;
    std::string workName;
    if (!BuildSyncRows(e, projPath, &rows, &workName)) return "{ \"error\": \"sync report failed\" }\n";
    std::vector<MincFootagePatch> strip;
    std::string harvestItems;
    bool first = true;
    for (auto &r : rows) {
        if (r.ok) continue;
        std::string cleaned;
        for (char c : r.current) if (c != '"') cleaned += c;   /* :736 quote strip */
        if (!first) harvestItems += ",";
        harvestItems += "\"" + std::to_string(r.id) + "\":{\"name\":\"" + cleaned + "\"}";
        first = false;
        MincFootagePatch fp;
        fp.id = r.id;
        fp.profileJSON = MincRifxProfileJSON(pr.working.c_str(), pr.family.c_str());
        strip.push_back(fp);
    }
    std::string harvest = "{ \"migrated\": \"" + presetKey + "\", \"items\": {" + harvestItems + "} }";
    if (!SaveTo(e, projPath)) return "{ \"error\": \"save failed\" }\n";
    std::string berr, bpath = BackupCopy(projPath, "premigrate", &berr);
    if (bpath.empty()) return "{ \"error\": " + JStr(berr) + " }\n";
    std::vector<MincXmpUpsert> xmp = {
        { "harvest", harvest },
        { "provenance", ProvenanceRecord(presetKey, pr.config) },
    };
    std::string perr;
    if (!MincRifxPatchProject(projPath.c_str(), cfg.c_str(), pr.pwcsJSON.c_str(), strip, xmp, &perr))
        return "{ \"error\": " + JStr("patch failed: " + perr) + " }\n";

    /* reopen — every handle above is now dead; the env suites remain valid */
    if (!Reopen(e, projPath)) return "{ \"error\": \"reopen failed\" }\n";
    MincAuthorityRefreshBp(bp, id);
    MincWriteMenus(bp, id);                              /* menus follow the new pin immediately */

    int bpc = 0;
    SetBitDepth(e, pr.family, &bpc);
    MincSuggestCtx ctx = MincBuildSuggestCtx(presetKey, cfg);
    RebuildOut rb;
    bool touched = false;
    std::set<std::string> configLooks;                   /* per-look validity (native-look crash guard) */
    { std::vector<std::string> lv = MincConfigLooks(cfg); for (auto &l : lv) configLooks.insert(l); }
    RebuildEffects(e, ctx, configLooks, &rb, &touched);
    if (touched) MincSyncFromNames(bp, id);

    /* post-audit */
    std::vector<SyncRow> audit;
    std::string wn2;
    int residual = 0;
    if (BuildSyncRows(e, projPath, &audit, &wn2))
        for (auto &r : audit) if (!r.ok) ++residual;
    MincAuthorityRefreshBp(bp, id);
    MincAuthoritySnapshot snap = {};
    MincAuthorityGet(&snap);
    std::string pinNow = snap.configPath;
    const char *CENTRAL = "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor";
    std::string locus = (pinNow.compare(0, strlen(CENTRAL), CENTRAL) == 0) ? "central" : "sidecar (central store not installed)";
    /* backups stats (:658-666) */
    int bcount = 0; double bmb = 0;
    {
        std::string bdir = Dirname(projPath) + "/_minColor/backups";
        DIR *dd = opendir(bdir.c_str());
        if (dd) {
            struct dirent *de;
            while ((de = readdir(dd)) != nullptr) {
                std::string n = de->d_name;
                if (n.size() > 4 && n.compare(n.size() - 4, 4, ".aep") == 0) {
                    ++bcount;
                    struct stat st;
                    if (stat((bdir + "/" + n).c_str(), &st) == 0) bmb += st.st_size / 1048576.0;
                }
            }
            closedir(dd);
        }
        bmb = (double)((long)(bmb * 10 + 0.5)) / 10.0;
    }
    char nums[256];
    snprintf(nums, sizeof(nums),
             ", \"stripped\": %d, \"residual\": %d, \"harvested\": %d, \"effectsRebuilt\": %d, \"viewRenderRetargeted\": %d, \"bitsPerChannel\": %d",
             (int)strip.size(), residual, (int)strip.size(), (int)rb.rebuilt.size(), rb.viewRender, bpc);
    char bstats[64];
    snprintf(bstats, sizeof(bstats), "{ \"count\": %d, \"mb\": %.1f }", bcount, bmb);
    return "{ \"preset\": " + JStr(presetKey) + ", \"working\": " + JStr(pr.workingSpaceLabel) +
           ", \"pinLocus\": " + JStr(locus) + nums +
           ", \"backup\": " + JStr(bpath) +
           ", \"effectsFailed\": " + JArr(rb.failed) +
           ", \"effectsRemapped\": " + JArr(rb.remapped) +
           ", \"effectsRemoved\": " + JArr(rb.removed) +
           ", \"utility\": null" +
           ", \"effectsForeign\": " + JArr(rb.foreign) +
           ", \"strippedPipeline\": " + JArr(rb.strippedPipeline) +
           ", \"gradesLeft\": " + JArr(rb.gradesLeft) +
           ", \"orphanLayers\": " + JArr(rb.orphans) +
           ", \"backups\": " + bstats + " }\n";
}

std::string MincPackageForAnyAE(SPBasicSuite *bp, AEGP_PluginID id) {
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ProjSuite6> pjs(bp, kAEGPProjSuite, kAEGPProjSuiteVersion6);
    Acq<AEGP_ItemSuite9> its(bp, kAEGPItemSuite, kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12> cps(bp, kAEGPCompSuite, kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9> lys(bp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    Acq<AEGP_FootageSuite5> fts(bp, kAEGPFootageSuite, kAEGPFootageSuiteVersion5);
    Acq<AEGP_UtilitySuite6> uts(bp, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    PEnv e;
    if (!AcquireEnv(bp, id, suites, e, pjs, its, cps, lys, fts, uts)) return "{ \"error\": \"suite acquire failed\" }\n";
    std::string projPath = ProjPath(e);
    if (projPath.empty()) return "{ \"error\": \"save the project first\" }\n";

    MincTranslateReport tr = MincTranslateToNative(bp, id);          /* :491 */

    /* identity from the live pin (sidecarInfo semantics, :492) */
    MincAuthorityRefreshBp(bp, id);
    MincAuthoritySnapshot snap = {};
    MincAuthorityGet(&snap);
    std::string preset = MincPresetFromConfigBase(Basename2(snap.configPath));
    if (preset.empty()) return "{ \"error\": \"not a minColor project\" }\n";
    std::string cfgTarget, serr;
    if (!MincEnsureSidecar(projPath, preset, &cfgTarget, &serr))     /* :493 */
        return "{ \"error\": " + JStr(serr) + " }\n";

    /* the panel re-pins live and stamps engine="native" during translate (:640); the native
       path folds both into one patch ceremony — save (persists translated effects), patch
       pin + engine, reopen. Working space and footage untouched.                          */
    if (!SaveTo(e, projPath)) return "{ \"error\": \"save failed\" }\n";
    std::vector<MincFootagePatch> none;
    std::vector<MincXmpUpsert> xmp = { { "engine", "native" } };
    std::string perr;
    if (!MincRifxPatchProject(projPath.c_str(), cfgTarget.c_str(), nullptr, none, xmp, &perr))
        return "{ \"error\": " + JStr("patch failed: " + perr) + " }\n";
    if (!Reopen(e, projPath)) return "{ \"error\": \"reopen failed\" }\n";
    MincAuthorityRefreshBp(bp, id);
    MincWriteMenus(bp, id);                              /* menus follow the sidecar pin */

    std::string ar = MincArchiveProject(bp, id);                     /* :494 */
    while (!ar.empty() && ar[ar.size() - 1] == '\n') ar.erase(ar.size() - 1);

    char tn[16];
    snprintf(tn, sizeof(tn), "%d", (int)tr.converted.size());
    return std::string("{ \"translated\": ") + tn +
           ", \"failed\": " + JArr(tr.failed) +
           ", \"remapped\": " + JArr(tr.remapped) +
           ", \"removed\": " + JArr(tr.removed) +
           ", \"archive\": " + ar + " }\n";
}

/* Native Repair (M3 decision: BOTH shapes — the shell heals live for the instant UX; this
   command is the shell-less zero-bridge twin): same-hash re-point via the patch ceremony,
   save -> backup -> pin patch -> reopen -> re-diagnose. Never a different config.        */
std::string MincRepairProject(SPBasicSuite *bp, AEGP_PluginID id) {
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ProjSuite6> pjs(bp, kAEGPProjSuite, kAEGPProjSuiteVersion6);
    Acq<AEGP_ItemSuite9> its(bp, kAEGPItemSuite, kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12> cps(bp, kAEGPCompSuite, kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9> lys(bp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    Acq<AEGP_FootageSuite5> fts(bp, kAEGPFootageSuite, kAEGPFootageSuiteVersion5);
    Acq<AEGP_UtilitySuite6> uts(bp, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    PEnv e;
    if (!AcquireEnv(bp, id, suites, e, pjs, its, cps, lys, fts, uts)) return "{ \"error\": \"suite acquire failed\" }\n";
    std::string projPath = ProjPath(e);
    if (projPath.empty()) return "{ \"error\": \"save the project first\" }\n";
    MincAuthorityRefreshBp(bp, id);
    MincDoctorResult d = MincDoctorDiagnose(bp, id);
    if (d.status == "green") return "{ \"status\": \"green\", \"action\": \"none\" }\n";
    if (d.repairTarget.empty())
        return "{ \"error\": " + JStr("cannot repair (" + d.status + ": " + d.text + ") \xe2\x80\x94 run Set Up / Migrate") + " }\n";
    if (!SaveTo(e, projPath)) return "{ \"error\": \"save failed\" }\n";
    std::string berr, bpath = BackupCopy(projPath, "prerepair", &berr);
    if (bpath.empty()) return "{ \"error\": " + JStr(berr) + " }\n";
    std::vector<MincFootagePatch> none;
    std::vector<MincXmpUpsert> noXmp;
    std::string perr;
    if (!MincRifxPatchProject(projPath.c_str(), d.repairTarget.c_str(), nullptr, none, noXmp, &perr))
        return "{ \"error\": " + JStr("patch failed: " + perr) + " }\n";
    if (!Reopen(e, projPath)) return "{ \"error\": \"reopen failed\" }\n";
    MincAuthorityRefreshBp(bp, id);
    MincWriteMenus(bp, id);
    MincSyncFromNames(bp, id);                           /* panel repair's syncPluginNames */
    MincDoctorResult after = MincDoctorDiagnose(bp, id);
    return "{ \"status\": " + JStr(after.status) + ", \"repairedTo\": " + JStr(d.repairTarget) +
           ", \"backup\": " + JStr(bpath) + " }\n";
}
