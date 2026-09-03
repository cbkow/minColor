#include "MincStrip.h"
#include "MincEffectOps.h"
#include <vector>
#include <set>
#include <map>

static std::string JStr2(const std::string &s) {
    std::string o = "\"";
    for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
    return o + "\"";
}
static std::string JArr2(const std::vector<std::string> &v) {
    std::string o = "[";
    for (size_t i = 0; i < v.size(); ++i) { if (i) o += ", "; o += JStr2(v[i]); }
    return o + "]";
}

struct SEnv {
    SPBasicSuite *bp; AEGP_PluginID id;
    AEGP_SuiteHandler *suites;
    AEGP_ItemSuite9 *its; AEGP_CompSuite12 *cps; AEGP_LayerSuite9 *lys;
    bool all;
    std::vector<std::string> stripped, gradesLeft, contained, failed, layersRemoved;
};

static std::string SItemName(SEnv &e, AEGP_ItemH it) {
    AEGP_MemHandle h = nullptr; char b[512] = "";
    if (e.its->AEGP_GetItemName(e.id, it, &h) == A_Err_NONE)
        MincUtf16HandleToUtf8(*e.suites, h, b, sizeof(b));
    return b;
}
static std::string SLayerName(SEnv &e, AEGP_LayerH ly) {
    AEGP_MemHandle nh = nullptr, sh = nullptr; char b[512] = "";
    if (e.lys->AEGP_GetLayerName(e.id, ly, &nh, &sh) == A_Err_NONE) {
        MincUtf16HandleToUtf8(*e.suites, nh, b, sizeof(b));
        if (!b[0] && sh) MincUtf16HandleToUtf8(*e.suites, sh, b, sizeof(b));
        else if (sh) e.suites->MemorySuite1()->AEGP_FreeMemHandle(sh);
    }
    return b;
}
static bool ContainOn(SEnv &e, AEGP_LayerH ly) {                 /* :1295-1299 */
    std::vector<MincFxEntry> fx;
    MincEnumLayerEffects(e.bp, e.id, ly, &fx);
    for (auto &f : fx)
        if (f.name.compare(0, 18, "minColor: contain ") == 0) return true;
    return false;
}

static void SWalk(SEnv &e, AEGP_CompH compH, std::set<int32_t> &seen) {
    AEGP_ItemH item = nullptr;
    e.cps->AEGP_GetItemFromComp(compH, &item);
    A_long cid = 0;
    if (item) e.its->AEGP_GetItemID(item, &cid);
    if (seen.count((int32_t)cid)) return;
    seen.insert((int32_t)cid);
    std::string compName = item ? SItemName(e, item) : "";
    A_long nL = 0;
    e.lys->AEGP_GetCompNumLayers(compH, &nL);
    for (A_long L = 0; L < nL; ++L) {
        AEGP_LayerH ly = nullptr;
        if (e.lys->AEGP_GetCompLayerByIndex(compH, L, &ly) != A_Err_NONE || !ly) continue;
        std::string lyName = SLayerName(e, ly);
        std::string label = compName + "/" + lyName;
        if (e.all) {                                             /* utility-layer demolition (:1306-1312) */
            AEGP_LayerFlags fl = 0;
            e.lys->AEGP_GetLayerFlags(ly, &fl);
            bool utilName = lyName.compare(0, 13, "minColor VIEW") == 0 || lyName.compare(0, 15, "minColor RENDER") == 0;
            if ((fl & AEGP_LayerFlag_ADJUSTMENT_LAYER) && utilName) {
                if (fl & AEGP_LayerFlag_LOCKED) MincSetLayerLocked(e.bp, ly, false);
                if (e.lys->AEGP_DeleteLayer(ly) == A_Err_NONE) {
                    e.layersRemoved.push_back(label);
                    e.lys->AEGP_GetCompNumLayers(compH, &nL);
                    --L;
                    continue;
                }
                e.failed.push_back(label + " \xe2\x80\x94 layer remove failed");
            }
        }
        AEGP_ItemH src = nullptr;
        e.lys->AEGP_GetLayerSourceItem(ly, &src);
        std::vector<MincFxEntry> fx;
        MincEnumLayerEffects(e.bp, e.id, ly, &fx);
        for (int k = 0; k < (int)fx.size(); ++k) {
            const std::string &mn = fx[k].match, &nm = fx[k].name;
            bool isLegacy = (mn == MINC_MATCH_LEGACY);  /* M3 step 8: legacy is no longer live — */
            bool isMincLive = MincIsOurs(mn.c_str()) && !isLegacy;   /* ours = the four variants  */
            bool isPipeline = (mn == "ADBE OCIO Display Transform" || mn == "ADBE OCIO Look Transform" ||
                               mn == "ADBE OCIO Color Space Transform" || isMincLive || isLegacy);
            bool isGrade = (mn == "ADBE OCIO CDL Transform" || mn == "ADBE OCIO FILE Transform");
            bool doomed = e.all ? (isPipeline || isGrade)
                                : isLegacy ||               /* a placeholder is dead weight in BOTH modes */
                                  (isPipeline && !isMincLive && nm.compare(0, 10, "minColor: ") != 0);
            if (doomed) {
                bool wl = MincLayerLocked(e.bp, ly);
                if (wl) MincSetLayerLocked(e.bp, ly, false);
                bool ok = MincRemoveEffectAt(e.bp, e.id, ly, k + 1);
                if (wl) MincSetLayerLocked(e.bp, ly, true);
                if (ok) {                                 /* labels use the DISPLAY name (:1325) */
                    e.stripped.push_back(label + " [" + nm + "]" + (isGrade ? "  \xe2\x86\x90 FILE GRADE" : ""));
                    fx.erase(fx.begin() + k); --k;
                } else e.failed.push_back(label + " [" + nm + "] \xe2\x80\x94 remove failed");
            } else if (isGrade) {
                e.gradesLeft.push_back(label + " [" + nm + "]");
            }
        }
        if (src) {
            AEGP_ItemType ty = AEGP_ItemType_NONE;
            e.its->AEGP_GetItemType(src, &ty);
            if (ty == AEGP_ItemType_COMP) {
                if (!e.all && ContainOn(e, ly)) {
                    e.contained.push_back(label + " \xe2\x80\x94 interior is media, left untouched");
                    continue;
                }
                AEGP_CompH sub = nullptr;
                if (e.cps->AEGP_GetCompFromItem(src, &sub) == A_Err_NONE && sub) SWalk(e, sub, seen);
            }
        }
    }
}

std::string MincStripForeignOcio(SPBasicSuite *bp, AEGP_PluginID id, bool all) {
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ItemSuite9>  its(bp, kAEGPItemSuite,  kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12> cps(bp, kAEGPCompSuite,  kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9> lys(bp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    Acq<AEGP_UtilitySuite6> uts(bp, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    if (!its || !cps || !lys) return "{ \"error\": \"suite acquire failed\" }\n";
    AEGP_ItemH active = nullptr;
    its->AEGP_GetActiveItem(&active);
    AEGP_ItemType aty = AEGP_ItemType_NONE;
    if (active) its->AEGP_GetItemType(active, &aty);
    if (!active || aty != AEGP_ItemType_COMP) return "{ \"error\": \"open a comp\" }\n";   /* :1293 */
    AEGP_CompH comp = nullptr;
    cps->AEGP_GetCompFromItem(active, &comp);
    if (!comp) return "{ \"error\": \"open a comp\" }\n";
    SEnv e;
    e.bp = bp; e.id = id; e.suites = &suites;
    e.its = its.p; e.cps = cps.p; e.lys = lys.p; e.all = all;
    if (uts) uts->AEGP_StartUndoGroup(all ? "minColor strip ALL" : "minColor strip foreign");
    std::set<int32_t> seen;
    SWalk(e, comp, seen);
    if (uts) uts->AEGP_EndUndoGroup();
    return "{ \"stripped\": " + JArr2(e.stripped) + ", \"gradesLeft\": " + JArr2(e.gradesLeft) +
           ", \"contained\": " + JArr2(e.contained) + ", \"failed\": " + JArr2(e.failed) +
           ", \"layersRemoved\": " + JArr2(e.layersRemoved) + " }\n";
}

/* --- Pre-reopen migrate safety (2026-09-03): whole-project native-OCIO scan + foreign strip -----
   A project whose pinned OCIO config is unreachable opens in FALLBACK (OCIO off) — AE never
   validates its native OCIO effects. Migrate repins to a reachable config and REOPENS, which turns
   OCIO on and makes AE validate every native OCIO effect against ITS OWN baked config path,
   aborting inside OCIOWrapper on any that's unreachable (unmounted volume, deleted config) — the
   crash fires DURING the reopen, before RebuildEffects can clean up. Foreign native effects are
   exactly what migrate discards anyway, so stripping them here — while OCIO is still off, a purely
   structural removal that needs no config — lets the reopen proceed. Only FOREIGN natives are
   touched: legacy MINC placeholders (resurrected post-reopen) and minColor-named natives (retargeted
   post-reopen, or refused by the guard) are left alone.  See [[mincolor-2-0-direction]]. */

static bool SNativeOcio(const std::string &mn) { return mn.compare(0, 9, "ADBE OCIO") == 0; }
static bool SForeignNative(const MincFxEntry &f) {
    return SNativeOcio(f.match) && f.name.compare(0, 10, "minColor: ") != 0;
}

static void SScan(SEnv &e, AEGP_CompH compH, std::set<int32_t> &seen, int *foreign, int *mincOwned) {
    AEGP_ItemH item = nullptr; e.cps->AEGP_GetItemFromComp(compH, &item);
    A_long cid = 0; if (item) e.its->AEGP_GetItemID(item, &cid);
    if (seen.count((int32_t)cid)) return;
    seen.insert((int32_t)cid);
    A_long nL = 0; e.lys->AEGP_GetCompNumLayers(compH, &nL);
    for (A_long L = 0; L < nL; ++L) {
        AEGP_LayerH ly = nullptr;
        if (e.lys->AEGP_GetCompLayerByIndex(compH, L, &ly) != A_Err_NONE || !ly) continue;
        std::vector<MincFxEntry> fx; MincEnumLayerEffects(e.bp, e.id, ly, &fx);
        for (auto &f : fx) {
            if (!SNativeOcio(f.match)) continue;
            if (f.name.compare(0, 10, "minColor: ") == 0) ++*mincOwned; else ++*foreign;
        }
        AEGP_ItemH src = nullptr; e.lys->AEGP_GetLayerSourceItem(ly, &src);
        if (src) {
            AEGP_ItemType ty = AEGP_ItemType_NONE; e.its->AEGP_GetItemType(src, &ty);
            if (ty == AEGP_ItemType_COMP) {
                AEGP_CompH sub = nullptr;
                if (e.cps->AEGP_GetCompFromItem(src, &sub) == A_Err_NONE && sub) SScan(e, sub, seen, foreign, mincOwned);
            }
        }
    }
}

static void SStripForeign(SEnv &e, AEGP_CompH compH, std::set<int32_t> &seen) {
    AEGP_ItemH item = nullptr; e.cps->AEGP_GetItemFromComp(compH, &item);
    A_long cid = 0; if (item) e.its->AEGP_GetItemID(item, &cid);
    if (seen.count((int32_t)cid)) return;
    seen.insert((int32_t)cid);
    std::string compName = item ? SItemName(e, item) : "";
    A_long nL = 0; e.lys->AEGP_GetCompNumLayers(compH, &nL);
    for (A_long L = 0; L < nL; ++L) {
        AEGP_LayerH ly = nullptr;
        if (e.lys->AEGP_GetCompLayerByIndex(compH, L, &ly) != A_Err_NONE || !ly) continue;
        std::string label = compName + "/" + SLayerName(e, ly);
        std::vector<MincFxEntry> fx; MincEnumLayerEffects(e.bp, e.id, ly, &fx);
        for (int k = 0; k < (int)fx.size(); ++k) {
            if (!SForeignNative(fx[k])) continue;
            const std::string nm = fx[k].name;
            bool wl = MincLayerLocked(e.bp, ly);
            if (wl) MincSetLayerLocked(e.bp, ly, false);
            bool ok = MincRemoveEffectAt(e.bp, e.id, ly, k + 1);
            if (wl) MincSetLayerLocked(e.bp, ly, true);
            if (ok) { e.stripped.push_back(label + " [" + nm + "]"); fx.erase(fx.begin() + k); --k; }
            else e.failed.push_back(label + " [" + nm + "] \xe2\x80\x94 remove failed");
        }
        AEGP_ItemH src = nullptr; e.lys->AEGP_GetLayerSourceItem(ly, &src);
        if (src) {
            AEGP_ItemType ty = AEGP_ItemType_NONE; e.its->AEGP_GetItemType(src, &ty);
            if (ty == AEGP_ItemType_COMP) {
                AEGP_CompH sub = nullptr;
                if (e.cps->AEGP_GetCompFromItem(src, &sub) == A_Err_NONE && sub) SStripForeign(e, sub, seen);
            }
        }
    }
}

static bool SAcquireProj(SPBasicSuite *bp, AEGP_PluginID id, AEGP_SuiteHandler &suites, SEnv &e,
                         Acq<AEGP_ItemSuite9> &its, Acq<AEGP_CompSuite12> &cps, Acq<AEGP_LayerSuite9> &lys) {
    if (!its || !cps || !lys) return false;
    e.bp = bp; e.id = id; e.suites = &suites; e.its = its.p; e.cps = cps.p; e.lys = lys.p; e.all = false;
    return true;
}

void MincScanNativeOcio(SPBasicSuite *bp, AEGP_PluginID id, int *foreign, int *mincOwned) {
    if (foreign) *foreign = 0;
    if (mincOwned) *mincOwned = 0;
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ProjSuite6>  pjs(bp, kAEGPProjSuite,  kAEGPProjSuiteVersion6);
    Acq<AEGP_ItemSuite9>  its(bp, kAEGPItemSuite,  kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12> cps(bp, kAEGPCompSuite,  kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9> lys(bp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    SEnv e;
    if (!pjs || !SAcquireProj(bp, id, suites, e, its, cps, lys)) return;
    AEGP_ProjectH projH = nullptr;
    if (pjs->AEGP_GetProjectByIndex(0, &projH) != A_Err_NONE || !projH) return;
    int f = 0, m = 0; std::set<int32_t> seen;
    AEGP_ItemH it = nullptr; its->AEGP_GetFirstProjItem(projH, &it);
    while (it) {
        AEGP_ItemType ty = AEGP_ItemType_NONE; its->AEGP_GetItemType(it, &ty);
        if (ty == AEGP_ItemType_COMP) { AEGP_CompH c = nullptr; if (cps->AEGP_GetCompFromItem(it, &c) == A_Err_NONE && c) SScan(e, c, seen, &f, &m); }
        AEGP_ItemH nx = nullptr; its->AEGP_GetNextProjItem(projH, it, &nx); it = nx;
    }
    if (foreign) *foreign = f;
    if (mincOwned) *mincOwned = m;
}

std::string MincStripForeignNativeProject(SPBasicSuite *bp, AEGP_PluginID id) {
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ProjSuite6>    pjs(bp, kAEGPProjSuite,  kAEGPProjSuiteVersion6);
    Acq<AEGP_ItemSuite9>    its(bp, kAEGPItemSuite,  kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12>   cps(bp, kAEGPCompSuite,  kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9>   lys(bp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    Acq<AEGP_UtilitySuite6> uts(bp, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    SEnv e;
    if (!pjs || !SAcquireProj(bp, id, suites, e, its, cps, lys)) return "{ \"error\": \"suite acquire failed\" }\n";
    AEGP_ProjectH projH = nullptr;
    if (pjs->AEGP_GetProjectByIndex(0, &projH) != A_Err_NONE || !projH) return "{ \"error\": \"no project\" }\n";
    if (uts) uts->AEGP_StartUndoGroup("minColor pre-migrate foreign strip");
    std::set<int32_t> seen;
    AEGP_ItemH it = nullptr; its->AEGP_GetFirstProjItem(projH, &it);
    while (it) {
        AEGP_ItemType ty = AEGP_ItemType_NONE; its->AEGP_GetItemType(it, &ty);
        if (ty == AEGP_ItemType_COMP) { AEGP_CompH c = nullptr; if (cps->AEGP_GetCompFromItem(it, &c) == A_Err_NONE && c) SStripForeign(e, c, seen); }
        AEGP_ItemH nx = nullptr; its->AEGP_GetNextProjItem(projH, it, &nx); it = nx;
    }
    if (uts) uts->AEGP_EndUndoGroup();
    return "{ \"stripped\": " + JArr2(e.stripped) + ", \"failed\": " + JArr2(e.failed) + " }\n";
}
