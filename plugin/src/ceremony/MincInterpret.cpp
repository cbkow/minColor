#include "MincInterpret.h"
#include "MincSuggest.h"
#include "MincEffectOps.h"
#include "MincPresets.h"
#include "MincSettings.h"
#include "../core/MincRifx.h"
#include "MincJson.h"
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include "MincFs.h"

static std::string JsonArr(const std::vector<std::string> &v) {
    std::string o = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) o += ", ";
        std::string e = "\"";
        for (char c : v[i]) { if (c == '"' || c == '\\') e += '\\'; e += c; }
        o += e + "\"";
    }
    return o + "]";
}
std::string MincInterpretReport::toJson(void) const {
    std::string o = "{ \"added\": " + JsonArr(added) + ", \"skipped\": " + JsonArr(skipped) +
                    ", \"flagged\": " + JsonArr(flagged) + ", \"failed\": " + JsonArr(failed) +
                    ", \"identity\": " + JsonArr(identity) + ", \"contained\": " + JsonArr(contained);
    if (!error.empty()) {
        std::string e = "\"";
        for (char c : error) { if (c == '"' || c == '\\') e += '\\'; e += c; }
        o += ", \"error\": " + e + "\"";
    }
    return o + " }\n";
}

/* ---------------- helpers ---------------- */
struct IEnv {
    SPBasicSuite *bp; AEGP_PluginID id;
    AEGP_SuiteHandler *suites;
    AEGP_ItemSuite9 *its; AEGP_CompSuite12 *cps; AEGP_LayerSuite9 *lys;
    AEGP_FootageSuite5 *fts; AEGP_ProjSuite6 *pjs;
    MincSuggestCtx ctx;
    std::string pin;
    std::string cfgBase, working;                 /* passport stamped into every authored effect */
    std::string explicitSpace;                    /* selection mode: bypass the suggestion engine */
    std::map<int32_t, std::string> detected, harvest;
    MincInterpretReport *rep;
};

static std::string ItemName(IEnv &e, AEGP_ItemH it) {
    AEGP_MemHandle h = nullptr;
    char b[512] = "";
    if (e.its->AEGP_GetItemName(e.id, it, &h) == A_Err_NONE)
        MincUtf16HandleToUtf8(*e.suites, h, b, sizeof(b));
    return b;
}
static std::string LayerName(IEnv &e, AEGP_LayerH ly) {
    AEGP_MemHandle nh = nullptr, sh = nullptr;
    char b[512] = "";
    if (e.lys->AEGP_GetLayerName(e.id, ly, &nh, &sh) == A_Err_NONE) {
        MincUtf16HandleToUtf8(*e.suites, nh, b, sizeof(b));
        if (!b[0] && sh) { char s2[512] = ""; MincUtf16HandleToUtf8(*e.suites, sh, s2, sizeof(s2)); snprintf(b, sizeof(b), "%s", s2); }
        else if (sh) e.suites->MemorySuite1()->AEGP_FreeMemHandle(sh);
    }
    return b;
}
static bool ContainsCI(const std::string &hay, const char *needle) {
    size_t n = strlen(needle);
    if (hay.size() < n) return false;
    for (size_t i = 0; i + n <= hay.size(); ++i) {
        size_t j = 0;
        while (j < n && tolower((unsigned char)hay[i + j]) == tolower((unsigned char)needle[j])) ++j;
        if (j == n) return true;
    }
    return false;
}
static bool IsPreCst(const MincFxEntry &fx) {                /* PRE_CST /EXtractoR|IDentifier|ProEXR|Cryptomatte/i */
    static const char *T[] = { "EXtractoR", "IDentifier", "ProEXR", "Cryptomatte" };
    for (int i = 0; i < 4; ++i)
        if (ContainsCI(fx.match, T[i]) || ContainsCI(fx.name, T[i])) return true;
    return false;
}
static int CstTargetIndex(const std::vector<MincFxEntry> &fx) {   /* :961-968 */
    int idx = 1;
    for (size_t ci = 0; ci < fx.size(); ++ci)
        if (IsPreCst(fx[ci])) idx = (int)ci + 2;             /* ci is 0-based; panel: idx = ci+1 (1-based)+1 */
    return idx;
}
static std::string ContainSpace(IEnv &e, AEGP_LayerH ly) {   /* :1020-1024 */
    std::vector<MincFxEntry> fx;
    MincEnumLayerEffects(e.bp, e.id, ly, &fx);
    const std::string pre = "minColor: contain ";
    for (auto &f : fx)
        if (f.name.compare(0, pre.size(), pre) == 0) return f.name.substr(pre.size());
    return "";
}

static MinColorArb InputArb(const std::string &space) {   /* interpret authors input (-> working) only */
    MinColorArb a; memset(&a, 0, sizeof(a));
    a.magic = MINC_ARB_MAGIC; a.version = MINC_ARB_VERSION; a.direction = MINC_DIR_TO_WORKING;
    snprintf(a.space, MINC_SPACE_LEN, "%s", space.c_str());
    return a;
}

static void DoLayer(IEnv &e, AEGP_CompH compH, AEGP_LayerH ly, const std::string &compName) {
    AEGP_ItemH src = nullptr;
    if (e.lys->AEGP_GetLayerSourceItem(ly, &src) != A_Err_NONE || !src) return;   /* camera/light */
    AEGP_ItemType ty = AEGP_ItemType_NONE;
    e.its->AEGP_GetItemType(src, &ty);
    if (ty != AEGP_ItemType_FOOTAGE) return;
    /* inScope: FileSource with a file (:954) */
    AEGP_FootageH ftg = nullptr;
    if (e.fts->AEGP_GetMainFootageFromItem(src, &ftg) != A_Err_NONE || !ftg) return;
    char fpath[2048] = "";
    {
        AEGP_MemHandle ph = nullptr;
        if (e.fts->AEGP_GetFootagePath(ftg, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, &ph) == A_Err_NONE)
            MincUtf16HandleToUtf8(*e.suites, ph, fpath, sizeof(fpath));
    }
    if (!fpath[0]) return;
    std::string label = compName + "/" + LayerName(e, ly);
    /* facts */
    MincItemFacts facts;
    A_long iid = 0; e.its->AEGP_GetItemID(src, &iid);
    facts.id = (int32_t)iid;
    std::string fp = fpath;
    size_t sl = fp.find_last_of('/');
    facts.fileName = sl == std::string::npos ? fp : fp.substr(sl + 1);
    AEGP_ItemFlags ifl = 0; e.its->AEGP_GetItemFlags(src, &ifl);
    facts.isStill = (ifl & AEGP_ItemFlag_STILL) != 0;

    std::vector<MincFxEntry> fx;
    MincEnumLayerEffects(e.bp, e.id, ly, &fx);
    /* existing input CST (:974-975): FX_PREFIX name + (native CST | legacy | XFORM).
       INPUT transforms only — a VIEW/RENDER/LOOK variant here must never read as
       "interpreted" (G1/G2 narrowing, plan P4).                                    */
    int haveIdx = -1;
    for (size_t fi = 0; fi < fx.size(); ++fi)
        if (fx[fi].name.compare(0, 10, "minColor: ") == 0 &&
            (fx[fi].match == "ADBE OCIO Color Space Transform" ||
             fx[fi].match == MINC_MATCH_LEGACY || fx[fi].match == MINC_MATCH_XFORM)) { haveIdx = (int)fi + 1; break; }
    if (haveIdx > 0) {                                       /* self-heal (:976-993) */
        MincFxName pr0 = MincParseFxName(fx[haveIdx - 1].name);
        MincRemap r0;
        if (pr0.valid && pr0.kind == "input") r0 = MincRemapSpace("input", pr0.space, e.ctx);
        else { r0.changed = true; r0.note = "(unparsed CST name)"; }
        if (r0.changed) {
            bool wl0 = MincLayerLocked(e.bp, ly);
            if (wl0) MincSetLayerLocked(e.bp, ly, false);
            if (!r0.space.empty() && fx[haveIdx - 1].match == MINC_MATCH_XFORM) {
                /* rename-in-place is XFORM-only (M3 step 8): renaming a legacy PLACEHOLDER
                   would leave a dead effect wearing a live name — legacy falls through to
                   remove + re-author below (interpret's resurrection)                     */
                MincRenameEffectAt(e.bp, e.id, ly, haveIdx, "minColor: " + r0.space + " \xe2\x86\x92 working");
                { MinColorArb ar = InputArb(r0.space);
                  MincReauthorEffectAt(e.bp, e.id, ly, haveIdx, &ar, e.cfgBase.c_str(), e.working.c_str()); }
                if (wl0) MincSetLayerLocked(e.bp, ly, true);
                e.rep->flagged.push_back(label + " \xe2\x80\x94 renamed " + r0.note);
                return;
            }
            MincRemoveEffectAt(e.bp, e.id, ly, haveIdx);
            if (!r0.space.empty()) {
                { MinColorArb ar = InputArb(r0.space);
                  MincApplyMincSelfContained(e.bp, e.id, ly, "minColor: " + r0.space + " \xe2\x86\x92 working", 0,
                                             &ar, e.cfgBase.c_str(), e.working.c_str()); }
                if (wl0) MincSetLayerLocked(e.bp, ly, true);
                e.rep->flagged.push_back(label + " \xe2\x80\x94 rebuilt " + r0.note);
                return;
            }
            if (wl0) MincSetLayerLocked(e.bp, ly, true);
            if (r0.identity) { e.rep->identity.push_back(label + " (" + r0.note + ")"); return; }
            e.rep->flagged.push_back(label + " \xe2\x80\x94 removed stale CST " + r0.note);
            haveIdx = -1;
            MincEnumLayerEffects(e.bp, e.id, ly, &(fx = {}));
        }
    }
    int tgt = CstTargetIndex(fx);
    if (haveIdx > 0 && haveIdx != tgt) {                     /* position repair (:996-1000) */
        bool wl = MincLayerLocked(e.bp, ly);
        if (wl) MincSetLayerLocked(e.bp, ly, false);
        MincMoveEffect(e.bp, e.id, ly, haveIdx, haveIdx < tgt ? tgt - 1 : tgt);
        if (wl) MincSetLayerLocked(e.bp, ly, true);
        e.rep->added.push_back(label + (tgt > 1 ? " (existing CST moved after channel extractor)" : " (existing CST moved to top)"));
        return;
    }
    if (haveIdx > 0) { e.rep->skipped.push_back(label + " (already interpreted)"); return; }
    MincPick pick;                                           /* :1003 — explicit space wins (panel parity) */
    if (!e.explicitSpace.empty()) { pick.space = e.explicitSpace; pick.why = "explicit"; }
    else pick = MincSuggestionFor(facts, e.detected, e.harvest, e.ctx);
    if (pick.space.empty()) {
        if (pick.why.find("identity") != std::string::npos) e.rep->identity.push_back(label + " (" + pick.why + ")");
        else e.rep->skipped.push_back(label + " (" + pick.why + ")");
        return;
    }
    if (!MincSpaceInPin(pick.space, e.pin)) {                /* assertSpaceInPin throw -> failed bucket (:1018) */
        e.rep->failed.push_back(label + " \xe2\x80\x94 Error: '" + pick.space + "' is not in this project's pinned config");
        return;
    }
    bool wasLocked = MincLayerLocked(e.bp, ly);
    if (wasLocked) MincSetLayerLocked(e.bp, ly, false);
    if (!fx.empty())
        e.rep->flagged.push_back(label + " \xe2\x80\x94 existing effects present; CST placed " +
                                 (tgt > 1 ? "after the channel extractor" : "at the top") + ", review order");
    MinColorArb a = InputArb(pick.space);
    bool ok = MincApplyMincSelfContained(e.bp, e.id, ly, "minColor: " + pick.space + " \xe2\x86\x92 working", tgt,
                                         &a, e.cfgBase.c_str(), e.working.c_str());
    if (wasLocked) MincSetLayerLocked(e.bp, ly, true);
    if (!ok) { e.rep->failed.push_back(label + " \xe2\x80\x94 apply failed"); return; }
    e.rep->added.push_back(label + " \xe2\x86\x90 " + pick.space + " [" + pick.why + "]");
}

static void WalkComp(IEnv &e, AEGP_CompH compH, std::set<int32_t> &seen) {
    AEGP_ItemH item = nullptr;
    e.cps->AEGP_GetItemFromComp(compH, &item);
    A_long cid = 0;
    if (item) e.its->AEGP_GetItemID(item, &cid);
    if (seen.count((int32_t)cid)) return;
    seen.insert((int32_t)cid);
    std::string compName = item ? ItemName(e, item) : "";
    A_long nL = 0;
    e.lys->AEGP_GetCompNumLayers(compH, &nL);
    for (A_long li = 0; li < nL; ++li) {
        AEGP_LayerH ly = nullptr;
        if (e.lys->AEGP_GetCompLayerByIndex(compH, li, &ly) != A_Err_NONE || !ly) continue;
        AEGP_ItemH src = nullptr;
        e.lys->AEGP_GetLayerSourceItem(ly, &src);
        AEGP_ItemType ty = AEGP_ItemType_NONE;
        if (src) e.its->AEGP_GetItemType(src, &ty);
        if (src && ty == AEGP_ItemType_COMP) {
            std::string csp = ContainSpace(e, ly);
            if (!csp.empty()) { e.rep->contained.push_back(compName + "/" + LayerName(e, ly) + " \xe2\x86\x90 " + csp); continue; }
            AEGP_CompH sub = nullptr;
            if (e.cps->AEGP_GetCompFromItem(src, &sub) == A_Err_NONE && sub) WalkComp(e, sub, seen);
        } else if (src && ty == AEGP_ItemType_FOOTAGE) {
            DoLayer(e, compH, ly, compName);
        }
    }
}

MincInterpretReport MincInterpretTimeline(SPBasicSuite *bp, AEGP_PluginID id) {
    MincInterpretReport rep;
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ItemSuite9>   its(bp, kAEGPItemSuite,    kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12>  cps(bp, kAEGPCompSuite,    kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9>  lys(bp, kAEGPLayerSuite,   kAEGPLayerSuiteVersion9);
    Acq<AEGP_FootageSuite5> fts(bp, kAEGPFootageSuite, kAEGPFootageSuiteVersion5);
    Acq<AEGP_ProjSuite6>   pjs(bp, kAEGPProjSuite,    kAEGPProjSuiteVersion6);
    Acq<AEGP_UtilitySuite6> uts(bp, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    if (!its || !cps || !lys || !fts || !pjs) { rep.error = "suite acquire failed"; return rep; }
    AEGP_ItemH active = nullptr;
    its->AEGP_GetActiveItem(&active);
    AEGP_ItemType aty = AEGP_ItemType_NONE;
    if (active) its->AEGP_GetItemType(active, &aty);
    if (!active || aty != AEGP_ItemType_COMP) {
        rep.error = "open a comp (timeline pass starts from the active comp)";   /* :1058 verbatim */
        return rep;
    }
    AEGP_CompH comp = nullptr;
    cps->AEGP_GetCompFromItem(active, &comp);
    if (!comp) { rep.error = "open a comp (timeline pass starts from the active comp)"; return rep; }

    IEnv e;
    e.bp = bp; e.id = id; e.suites = &suites;
    e.its = its.p; e.cps = cps.p; e.lys = lys.p; e.fts = fts.p; e.pjs = pjs.p;
    e.rep = &rep;
    /* authority + ctx */
    MincAuthorityRefreshBp(bp, id);
    MincAuthoritySnapshot snap = {};
    MincAuthorityGet(&snap);
    std::string fullBase;                                    /* lean-v3 Path 2: author against the FULL config */
    std::string fullPath = MincEffectConfigPath(snap.configPath, "", &fullBase);
    e.pin = fullPath;                                        /* FULL config path — space-checks (:197) + ctx, NOT AE's interface pin */
    std::string preset = MincPresetFromConfigBase(fullBase);
    e.ctx = MincBuildSuggestCtx(preset, fullPath);           /* suggestions read the FULL space list */
    e.cfgBase = fullBase;                                    /* passport = full config basename */
    e.working = snap.workingSpace[0] ? std::string(snap.workingSpace)
                                     : MincPresetMeta(preset).working;
    /* detected + harvest: TEMP-COPY save (probe-E copy semantics — the user's file is untouched;
       mirrors the panel's data without its force-save) */
    {
        AEGP_ProjectH projH = nullptr;
        pjs->AEGP_GetProjectByIndex(0, &projH);
        char pbuf[2048] = "";
        if (projH) {
            AEGP_MemHandle ph = nullptr;
            if (pjs->AEGP_GetProjectPath(projH, &ph) == A_Err_NONE)
                MincUtf16HandleToUtf8(suites, ph, pbuf, sizeof(pbuf));
        }
        if (pbuf[0] && projH) {                              /* unsaved project -> empty maps (panel parity) */
            std::string tmp = mfs::tempPath("minColor-interpret-scan.aep");
            A_UTF16Char t16[512];
            MincU8ToU16(tmp.c_str(), t16, 512);
            if (pjs->AEGP_SaveProjectToPath(projH, t16) == A_Err_NONE) {
                MincAssignments a;
                if (MincRifxReadAssignments(tmp.c_str(), &a))
                    for (auto &kv : a.detected) e.detected[kv.first] = kv.second;
                std::string harv = MincXmpReadElement(MincReadFileTail(tmp.c_str()), "harvest");
                if (!harv.empty()) {
                    MincJsonPtr hj = MincJsonParse(harv);
                    MincJsonPtr items = hj ? hj->get("items") : nullptr;
                    if (items && items->type == MincJsonValue::Object)
                        for (auto &kv : items->obj)
                            if (kv.second) e.harvest[(int32_t)atol(kv.first.c_str())] = kv.second->str("name");
                }
                mfs::removeFile(tmp);
            }
        }
    }
    if (uts) uts->AEGP_StartUndoGroup("minColor interpret");
    std::set<int32_t> seen;
    WalkComp(e, comp, seen);
    if (uts) uts->AEGP_EndUndoGroup();
    /* lean-v3: effects author their own arb at apply time (self-contained) — no name-walk */
    return rep;
}

/* interpretPass({mode:"selection"}, space) port (:1051-1054): the active comp's SELECTED
   layers only; an explicit space bypasses the suggestion engine (why="explicit") and skips
   the detected/harvest scan (panel parity: those maps are empty when space is given).      */
MincInterpretReport MincInterpretSelection(SPBasicSuite *bp, AEGP_PluginID id, const std::string &space) {
    MincInterpretReport rep;
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ItemSuite9>    its(bp, kAEGPItemSuite,    kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12>   cps(bp, kAEGPCompSuite,    kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9>   lys(bp, kAEGPLayerSuite,   kAEGPLayerSuiteVersion9);
    Acq<AEGP_FootageSuite5> fts(bp, kAEGPFootageSuite, kAEGPFootageSuiteVersion5);
    Acq<AEGP_ProjSuite6>    pjs(bp, kAEGPProjSuite,    kAEGPProjSuiteVersion6);
    Acq<AEGP_UtilitySuite6> uts(bp, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    Acq<AEGP_CollectionSuite2> cls(bp, kAEGPCollectionSuite, kAEGPCollectionSuiteVersion2);
    if (!its || !cps || !lys || !fts || !pjs || !cls) { rep.error = "suite acquire failed"; return rep; }
    AEGP_ItemH active = nullptr;
    its->AEGP_GetActiveItem(&active);
    AEGP_ItemType aty = AEGP_ItemType_NONE;
    if (active) its->AEGP_GetItemType(active, &aty);
    if (!active || aty != AEGP_ItemType_COMP) { rep.error = "open a comp"; return rep; }   /* :1052 verbatim */
    AEGP_CompH comp = nullptr;
    cps->AEGP_GetCompFromItem(active, &comp);
    if (!comp) { rep.error = "open a comp"; return rep; }
    AEGP_Collection2H coll = nullptr;
    if (cps->AEGP_GetNewCollectionFromCompSelection(id, comp, &coll) != A_Err_NONE || !coll) {
        rep.error = "select layer(s)";                       /* :1053 verbatim */
        return rep;
    }
    A_u_long nSel = 0;
    cls->AEGP_GetCollectionNumItems(coll, &nSel);
    std::vector<AEGP_LayerH> layers;
    for (A_u_long i = 0; i < nSel; ++i) {
        AEGP_CollectionItemV2 item;
        memset(&item, 0, sizeof(item));
        if (cls->AEGP_GetCollectionItemByIndex(coll, i, &item) != A_Err_NONE) continue;
        if (item.type == AEGP_CollectionItemType_LAYER && item.u.layer.layerH)
            layers.push_back(item.u.layer.layerH);
    }
    cls->AEGP_DisposeCollection(coll);
    if (layers.empty()) { rep.error = "select layer(s)"; return rep; }

    IEnv e;
    e.bp = bp; e.id = id; e.suites = &suites;
    e.its = its.p; e.cps = cps.p; e.lys = lys.p; e.fts = fts.p; e.pjs = pjs.p;
    e.rep = &rep;
    e.explicitSpace = space;
    MincAuthorityRefreshBp(bp, id);
    MincAuthoritySnapshot snap = {};
    MincAuthorityGet(&snap);
    std::string fullBase;                                    /* lean-v3 Path 2: author against the FULL config */
    std::string fullPath = MincEffectConfigPath(snap.configPath, "", &fullBase);
    e.pin = fullPath;                                        /* FULL config path — space-checks (:197) + ctx, NOT AE's interface pin */
    std::string preset = MincPresetFromConfigBase(fullBase);
    e.ctx = MincBuildSuggestCtx(preset, fullPath);           /* suggestions read the FULL space list */
    e.cfgBase = fullBase;                                    /* passport = full config basename */
    e.working = snap.workingSpace[0] ? std::string(snap.workingSpace)
                                     : MincPresetMeta(preset).working;
    if (space.empty()) {                                     /* suggestion path needs detected/harvest */
        AEGP_ProjectH projH = nullptr;
        pjs->AEGP_GetProjectByIndex(0, &projH);
        char pbuf[2048] = "";
        if (projH) {
            AEGP_MemHandle ph = nullptr;
            if (pjs->AEGP_GetProjectPath(projH, &ph) == A_Err_NONE)
                MincUtf16HandleToUtf8(suites, ph, pbuf, sizeof(pbuf));
        }
        if (pbuf[0] && projH) {
            std::string tmp = mfs::tempPath("minColor-interpret-scan.aep");
            A_UTF16Char t16[512];
            MincU8ToU16(tmp.c_str(), t16, 512);
            if (pjs->AEGP_SaveProjectToPath(projH, t16) == A_Err_NONE) {
                MincAssignments a;
                if (MincRifxReadAssignments(tmp.c_str(), &a))
                    for (auto &kv : a.detected) e.detected[kv.first] = kv.second;
                std::string harv = MincXmpReadElement(MincReadFileTail(tmp.c_str()), "harvest");
                if (!harv.empty()) {
                    MincJsonPtr hj = MincJsonParse(harv);
                    MincJsonPtr hitems = hj ? hj->get("items") : nullptr;
                    if (hitems && hitems->type == MincJsonValue::Object)
                        for (auto &kv : hitems->obj)
                            if (kv.second) e.harvest[(int32_t)atol(kv.first.c_str())] = kv.second->str("name");
                }
                mfs::removeFile(tmp);
            }
        }
    }
    std::string compName = ItemName(e, active);
    if (uts) uts->AEGP_StartUndoGroup("minColor interpret");
    for (auto ly : layers) DoLayer(e, comp, ly, compName);
    if (uts) uts->AEGP_EndUndoGroup();
    /* lean-v3: self-contained authoring at apply time — no name-walk */
    return rep;
}
