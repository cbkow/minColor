#include "MincTranslate.h"
#include "MincEffectOps.h"
#include "MincSuggest.h"
#include "MincPresets.h"
#include "MincSettings.h"
#include <set>

static bool THasLooks(const std::string &pin) {
    std::string s = MincReadTextFile(pin);
    size_t li = s.find("\nlooks:");
    if (li == std::string::npos) return false;
    return s.substr(li + 7, 4000).find("name:") != std::string::npos;
}

MincTranslateReport MincTranslateToNative(SPBasicSuite *bp, AEGP_PluginID id) {
    MincTranslateReport out;
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ProjSuite6>  pjs(bp, kAEGPProjSuite,  kAEGPProjSuiteVersion6);
    Acq<AEGP_ItemSuite9>  its(bp, kAEGPItemSuite,  kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12> cps(bp, kAEGPCompSuite,  kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9> lys(bp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    Acq<AEGP_UtilitySuite6> uts(bp, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    if (!pjs || !its || !cps || !lys) { out.failed.push_back("suite acquire failed"); return out; }
    MincAuthorityRefreshBp(bp, id);
    MincAuthoritySnapshot snap = {};
    MincAuthorityGet(&snap);
    std::string pin = snap.configPath;
    std::string pinBase = pin.substr(pin.find_last_of('/') == std::string::npos ? 0 : pin.find_last_of('/') + 1);
    MincSuggestCtx ctx = MincBuildSuggestCtx(MincPresetFromConfigBase(pinBase), pin);
    bool hasLooks = THasLooks(pin);
    std::set<std::string> configLooks;                   /* per-look validity: never author a native
                                                            Look Transform for a look absent from the
                                                            config — that is the crash landmine (§41). */
    { std::vector<std::string> lv = MincConfigLooks(pin); for (auto &l : lv) configLooks.insert(l); }
    if (uts) uts->AEGP_StartUndoGroup("minColor translate to native");
    AEGP_ProjectH projH = nullptr;
    pjs->AEGP_GetProjectByIndex(0, &projH);
    AEGP_ItemH itemH = nullptr;
    its->AEGP_GetFirstProjItem(projH, &itemH);
    while (itemH) {
        AEGP_ItemType ty = AEGP_ItemType_NONE;
        its->AEGP_GetItemType(itemH, &ty);
        if (ty == AEGP_ItemType_COMP) {
            AEGP_CompH compH = nullptr;
            if (cps->AEGP_GetCompFromItem(itemH, &compH) == A_Err_NONE && compH) {
                AEGP_MemHandle inh = nullptr;
                char compName[512] = "";
                if (its->AEGP_GetItemName(id, itemH, &inh) == A_Err_NONE)
                    MincUtf16HandleToUtf8(suites, inh, compName, sizeof(compName));
                A_long nL = 0;
                lys->AEGP_GetCompNumLayers(compH, &nL);
                for (A_long li = 0; li < nL; ++li) {
                    AEGP_LayerH ly = nullptr;
                    if (lys->AEGP_GetCompLayerByIndex(compH, li, &ly) != A_Err_NONE || !ly) continue;
                    AEGP_MemHandle lnh = nullptr, lsh = nullptr;
                    char lyName[512] = "";
                    if (lys->AEGP_GetLayerName(id, ly, &lnh, &lsh) == A_Err_NONE) {
                        MincUtf16HandleToUtf8(suites, lnh, lyName, sizeof(lyName));
                        if (!lyName[0] && lsh) MincUtf16HandleToUtf8(suites, lsh, lyName, sizeof(lyName));
                        else if (lsh) suites.MemorySuite1()->AEGP_FreeMemHandle(lsh);
                    }
                    std::vector<MincFxEntry> fx;
                    MincEnumLayerEffects(bp, id, ly, &fx);
                    for (int k = 0; k < (int)fx.size(); ++k) {
                        const std::string nm = fx[k].name;   /* copy — branches below rewrite fx[k] */
                        if (nm.compare(0, 10, "minColor: ") != 0) continue;      /* :567 */
                        MincVerb tv = MINC_VERB_LEGACY;
                        if (!MincMatchVerb(fx[k].match.c_str(), &tv)) continue;  /* plugin->native only (all five ours) */
                        std::string label = std::string(compName) + "/" + lyName + " [" + nm + "]";
                        bool wl = MincLayerLocked(bp, ly);
                        if (wl) MincSetLayerLocked(bp, ly, false);
                        std::string rest = nm.substr(10);
                        if (rest.compare(0, 5, "look ") == 0) {                  /* look branch :596-610 */
                            if (!MincKindMatchesVerb("look", tv)) {
                                out.skipped.push_back(label + " (verb contradicts match \xe2\x80\x94 left as plugin effect)");
                                if (wl) MincSetLayerLocked(bp, ly, true);
                                continue;
                            }
                            std::string look = rest.substr(5);
                            if (!configLooks.count(look)) {    /* THIS look absent -> never author native */
                                if (MincRemoveEffectAt(bp, id, ly, k + 1)) {
                                    out.removed.push_back(label + " \xe2\x80\x94 look not in this preset's config");
                                    fx.erase(fx.begin() + k); --k;
                                } else out.failed.push_back(label + " \xe2\x80\x94 remove failed");
                                if (wl) MincSetLayerLocked(bp, ly, true);
                                continue;
                            }
                            int idx = k + 1;
                            if (!MincRemoveEffectAt(bp, id, ly, idx)) { out.failed.push_back(label + " \xe2\x80\x94 remove failed"); if (wl) MincSetLayerLocked(bp, ly, true); continue; }
                            /* popups FIRST (ref is position-bound to the parade end), move LAST */
                            int endIdx = 0;
                            AEGP_EffectRefH nEff = MincApplyByMatchWithName(bp, id, ly, "ADBE OCIO Look Transform", nm, &endIdx);
                            std::string perr;
                            if (!nEff) {               /* removed but not replaced: parade shifted down */
                                out.failed.push_back(label + " \xe2\x80\x94 native apply failed");
                                fx.erase(fx.begin() + k); --k;
                            } else if (MincSetPopupByName(bp, id, nEff, 2, look, &perr)) {
                                out.converted.push_back(label);
                                fx[k].match = "ADBE OCIO Look Transform";   /* replaced IN PLACE — positions unchanged */
                            } else {
                                out.failed.push_back(label + " \xe2\x80\x94 " + perr);
                                fx[k].match = "ADBE OCIO Look Transform";   /* native effect exists; popup default */
                            }
                            if (nEff) {
                                { Acq<AEGP_EffectSuite5> efs2(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5); if (efs2) efs2->AEGP_DisposeEffect(nEff); }
                                if (endIdx != idx) MincMoveEffect(bp, id, ly, endIdx, idx);
                            }
                            if (wl) MincSetLayerLocked(bp, ly, true);
                            continue;
                        }
                        MincFxName pn = MincParseFxName(nm);                     /* :611-612 */
                        if (!pn.valid) { out.skipped.push_back(label + " (unparsed name)"); if (wl) MincSetLayerLocked(bp, ly, true); continue; }
                        if (!MincKindMatchesVerb(pn.kind.c_str(), tv)) {
                            out.skipped.push_back(label + " (verb contradicts match \xe2\x80\x94 left as plugin effect)");
                            if (wl) MincSetLayerLocked(bp, ly, true);
                            continue;
                        }
                        MincRemap rn = MincRemapSpace(pn.kind, pn.space, ctx);   /* :613 */
                        if (rn.space.empty() && rn.identity) {
                            if (MincRemoveEffectAt(bp, id, ly, k + 1)) {
                                out.removed.push_back(label + " \xe2\x80\x94 " + rn.note);
                                fx.erase(fx.begin() + k); --k;
                            } else out.failed.push_back(label + " \xe2\x80\x94 remove failed");
                            if (wl) MincSetLayerLocked(bp, ly, true);
                            continue;
                        }
                        if (rn.space.empty()) {                                  /* never destroy without replacement :615 */
                            out.skipped.push_back(label + " (" + rn.note + " \xe2\x80\x94 left as plugin effect)");
                            if (wl) MincSetLayerLocked(bp, ly, true);
                            continue;
                        }
                        if (rn.changed) out.remapped.push_back(label + ": " + rn.note);
                        std::string src2, dst2;                                  /* :617 */
                        if (pn.kind == "contain" || pn.kind == "input") { src2 = rn.space; dst2 = "default"; }
                        else { src2 = "default"; dst2 = rn.space; }
                        int idx2 = k + 1;
                        MincLog("translate: removing idx=%d", idx2);
                        if (!MincRemoveEffectAt(bp, id, ly, idx2)) { out.failed.push_back(label + " \xe2\x80\x94 remove failed"); if (wl) MincSetLayerLocked(bp, ly, true); continue; }
                        MincLog("translate: removed, applying native");
                        std::string newName = (pn.kind == "input")                       /* fxName port (:781) */
                            ? "minColor: " + rn.space + " \xe2\x86\x92 working"
                            : "minColor: " + pn.kind + " " + rn.space;
                        /* popups FIRST (ref is position-bound to the parade end), move LAST */
                        int endIdx2 = 0;
                        AEGP_EffectRefH nEff = MincApplyByMatchWithName(bp, id, ly, "ADBE OCIO Color Space Transform", newName, &endIdx2);
                        std::string e1, e2;
                        if (!nEff) {               /* removed but not replaced: parade shifted down */
                            out.failed.push_back(label + " \xe2\x80\x94 native apply failed");
                            fx.erase(fx.begin() + k); --k;
                        } else if (MincSetPopupByName(bp, id, nEff, 1, src2, &e1) && MincSetPopupByName(bp, id, nEff, 2, dst2, &e2)) {
                            out.converted.push_back(label);
                            fx[k].match = "ADBE OCIO Color Space Transform";   /* replaced IN PLACE */
                            fx[k].name = newName;
                        } else {
                            out.failed.push_back(label + " \xe2\x80\x94 " + (e1.empty() ? e2 : e1));
                            fx[k].match = "ADBE OCIO Color Space Transform";
                            fx[k].name = newName;
                        }
                        if (nEff) {
                            { Acq<AEGP_EffectSuite5> efs3(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5); if (efs3) efs3->AEGP_DisposeEffect(nEff); }
                            if (endIdx2 != idx2) MincMoveEffect(bp, id, ly, endIdx2, idx2);
                        }
                        if (wl) MincSetLayerLocked(bp, ly, true);
                    }
                }
            }
        }
        AEGP_ItemH nextH = nullptr;
        its->AEGP_GetNextProjItem(projH, itemH, &nextH);
        itemH = nextH;
    }
    if (uts) uts->AEGP_EndUndoGroup();
    return out;
}

MincTranslateReport MincTranslateToPlugin(SPBasicSuite *bp, AEGP_PluginID id) {
    MincTranslateReport out;
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ProjSuite6>  pjs(bp, kAEGPProjSuite,  kAEGPProjSuiteVersion6);
    Acq<AEGP_ItemSuite9>  its(bp, kAEGPItemSuite,  kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12> cps(bp, kAEGPCompSuite,  kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9> lys(bp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    Acq<AEGP_UtilitySuite6> uts(bp, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    if (!pjs || !its || !cps || !lys) { out.failed.push_back("suite acquire failed"); return out; }
    MincAuthorityRefreshBp(bp, id);
    MincAuthoritySnapshot snap = {};
    MincAuthorityGet(&snap);
    std::string pin = snap.configPath;
    std::string pinBase = pin.substr(pin.find_last_of('/') == std::string::npos ? 0 : pin.find_last_of('/') + 1);
    MincSuggestCtx ctx = MincBuildSuggestCtx(MincPresetFromConfigBase(pinBase), pin);
    bool hasLooks = THasLooks(pin);
    if (uts) uts->AEGP_StartUndoGroup("minColor adopt");
    AEGP_ProjectH projH = nullptr;
    pjs->AEGP_GetProjectByIndex(0, &projH);
    AEGP_ItemH itemH = nullptr;
    its->AEGP_GetFirstProjItem(projH, &itemH);
    while (itemH) {
        AEGP_ItemType ty = AEGP_ItemType_NONE;
        its->AEGP_GetItemType(itemH, &ty);
        if (ty == AEGP_ItemType_COMP) {
            AEGP_CompH compH = nullptr;
            if (cps->AEGP_GetCompFromItem(itemH, &compH) == A_Err_NONE && compH) {
                AEGP_MemHandle inh = nullptr;
                char compName[512] = "";
                if (its->AEGP_GetItemName(id, itemH, &inh) == A_Err_NONE)
                    MincUtf16HandleToUtf8(suites, inh, compName, sizeof(compName));
                A_long nL = 0;
                lys->AEGP_GetCompNumLayers(compH, &nL);
                for (A_long li = 0; li < nL; ++li) {
                    AEGP_LayerH ly = nullptr;
                    if (lys->AEGP_GetCompLayerByIndex(compH, li, &ly) != A_Err_NONE || !ly) continue;
                    AEGP_MemHandle lnh = nullptr, lsh = nullptr;
                    char lyName[512] = "";
                    if (lys->AEGP_GetLayerName(id, ly, &lnh, &lsh) == A_Err_NONE) {
                        MincUtf16HandleToUtf8(suites, lnh, lyName, sizeof(lyName));
                        if (!lyName[0] && lsh) MincUtf16HandleToUtf8(suites, lsh, lyName, sizeof(lyName));
                        else if (lsh) suites.MemorySuite1()->AEGP_FreeMemHandle(lsh);
                    }
                    std::vector<MincFxEntry> fx;
                    MincEnumLayerEffects(bp, id, ly, &fx);
                    for (int k = 0; k < (int)fx.size(); ++k) {
                        const std::string nm = fx[k].name;
                        if (nm.compare(0, 10, "minColor: ") != 0) continue;      /* :567 */
                        bool isLook = (fx[k].match == "ADBE OCIO Look Transform");
                        bool isCst  = (fx[k].match == "ADBE OCIO Color Space Transform");
                        if (!isLook && !isCst) continue;                         /* native minColor-named only */
                        std::string label = std::string(compName) + "/" + lyName + " [" + nm + "]";
                        bool wl = MincLayerLocked(bp, ly);
                        if (wl) MincSetLayerLocked(bp, ly, false);
                        if (isLook) {                                            /* :572-581 */
                            if (!hasLooks) {
                                if (MincRemoveEffectAt(bp, id, ly, k + 1)) {
                                    out.removed.push_back(label + " \xe2\x80\x94 looks do not exist in this preset");
                                    fx.erase(fx.begin() + k); --k;
                                } else out.failed.push_back(label + " \xe2\x80\x94 remove failed");
                                if (wl) MincSetLayerLocked(bp, ly, true);
                                continue;
                            }
                            int idx = k + 1;
                            if (!MincRemoveEffectAt(bp, id, ly, idx)) { out.failed.push_back(label + " \xe2\x80\x94 remove failed"); if (wl) MincSetLayerLocked(bp, ly, true); continue; }
                            int endIdx = 0;
                            AEGP_EffectRefH lf = MincApplyByMatchWithName(bp, id, ly, MINC_MATCH_LOOK, nm, &endIdx);
                            if (lf) {
                                { Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5); if (efs) efs->AEGP_DisposeEffect(lf); }
                                if (endIdx != idx) MincMoveEffect(bp, id, ly, endIdx, idx);
                                out.converted.push_back(label);
                                fx[k].match = MINC_MATCH_LOOK;
                            } else { out.failed.push_back(label + " \xe2\x80\x94 variant apply failed"); fx.erase(fx.begin() + k); --k; }
                            if (wl) MincSetLayerLocked(bp, ly, true);
                            continue;
                        }
                        /* CST branch (:583-593): parse FIRST — the verb picks the variant */
                        MincFxName pp = MincParseFxName(nm);
                        MincRemap rp;
                        bool haveRemap = pp.valid;
                        if (haveRemap) rp = MincRemapSpace(pp.kind, pp.space, ctx);
                        std::string newName = nm;                                /* name kept unless remapped */
                        const char *targetMatch = MINC_MATCH_XFORM;              /* unparsed lives on XFORM */
                        if (pp.valid) targetMatch = MincMatchForKind(pp.kind.c_str());
                        if (haveRemap && rp.changed && !rp.space.empty()) {
                            out.remapped.push_back(label + ": " + rp.note);
                            newName = (pp.kind == "input")
                                ? "minColor: " + rp.space + " \xe2\x86\x92 working"
                                : "minColor: " + pp.kind + " " + rp.space;
                        } else if (haveRemap && rp.space.empty()) {
                            out.remapped.push_back(label + ": " + rp.note + " (name kept \xe2\x80\x94 re-interpret)");
                        }
                        int idx2 = k + 1;
                        if (!MincRemoveEffectAt(bp, id, ly, idx2)) { out.failed.push_back(label + " \xe2\x80\x94 remove failed"); if (wl) MincSetLayerLocked(bp, ly, true); continue; }
                        int endIdx2 = 0;
                        AEGP_EffectRefH nf = MincApplyByMatchWithName(bp, id, ly, targetMatch, newName, &endIdx2);
                        if (nf) {
                            { Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5); if (efs) efs->AEGP_DisposeEffect(nf); }
                            if (endIdx2 != idx2) MincMoveEffect(bp, id, ly, endIdx2, idx2);
                            out.converted.push_back(label);
                            fx[k].match = targetMatch;
                            fx[k].name = newName;
                        } else { out.failed.push_back(label + " \xe2\x80\x94 variant apply failed"); fx.erase(fx.begin() + k); --k; }
                        if (wl) MincSetLayerLocked(bp, ly, true);
                    }
                }
            }
        }
        AEGP_ItemH nextH = nullptr;
        its->AEGP_GetNextProjItem(projH, itemH, &nextH);
        itemH = nextH;
    }
    if (uts) uts->AEGP_EndUndoGroup();
    MincSyncFromNames(bp, id);                               /* panel: syncPluginNames + stampEngine("plugin");
                                                                the walk writes payloads; engine stamp = shell/XMP
                                                                concern deferred to the contract flip */
    return out;
}
