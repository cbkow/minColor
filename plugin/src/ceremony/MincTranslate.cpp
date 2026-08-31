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
                        if (fx[k].match != MINC_MATCH_NAME) continue;            /* plugin->native only */
                        std::string label = std::string(compName) + "/" + lyName + " [" + nm + "]";
                        bool wl = MincLayerLocked(bp, ly);
                        if (wl) MincSetLayerLocked(bp, ly, false);
                        std::string rest = nm.substr(10);
                        if (rest.compare(0, 5, "look ") == 0) {                  /* look branch :596-610 */
                            std::string look = rest.substr(5);
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
                            AEGP_EffectRefH nEff = MincApplyByMatchWithName(bp, id, ly, "ADBE OCIO Look Transform", nm, idx);
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
                            if (nEff) { Acq<AEGP_EffectSuite5> efs2(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5); if (efs2) efs2->AEGP_DisposeEffect(nEff); }
                            if (wl) MincSetLayerLocked(bp, ly, true);
                            continue;
                        }
                        MincFxName pn = MincParseFxName(nm);                     /* :611-612 */
                        if (!pn.valid) { out.skipped.push_back(label + " (unparsed name)"); if (wl) MincSetLayerLocked(bp, ly, true); continue; }
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
                        if (!MincRemoveEffectAt(bp, id, ly, idx2)) { out.failed.push_back(label + " \xe2\x80\x94 remove failed"); if (wl) MincSetLayerLocked(bp, ly, true); continue; }
                        std::string newName = (pn.kind == "input")                       /* fxName port (:781) */
                            ? "minColor: " + rn.space + " \xe2\x86\x92 working"
                            : "minColor: " + pn.kind + " " + rn.space;
                        AEGP_EffectRefH nEff = MincApplyByMatchWithName(bp, id, ly, "ADBE OCIO Color Space Transform", newName, idx2);
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
                        if (nEff) { Acq<AEGP_EffectSuite5> efs3(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5); if (efs3) efs3->AEGP_DisposeEffect(nEff); }
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
