#include "MincUtility.h"
#include "MincEffectOps.h"
#include "MincSuggest.h"
#include "MincPresets.h"
#include "../core/MincMenus.h"
#include <cstdio>

static std::string UJ(const std::string &s) {
    std::string o = "\"";
    for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
    return o + "\"";
}

MincUtilLayer MincFindUtilityLayer(SPBasicSuite *bp, AEGP_PluginID id, AEGP_CompH comp, const char *kind) {
    MincUtilLayer out;
    Acq<AEGP_CompSuite12> cps(bp, kAEGPCompSuite, kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9> lys(bp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    if (!cps || !lys) return out;
    std::string pre = std::string("minColor: ") + kind + " ";
    A_long nL = 0;
    lys->AEGP_GetCompNumLayers(comp, &nL);
    for (A_long li = 0; li < nL; ++li) {
        AEGP_LayerH ly = nullptr;
        if (lys->AEGP_GetCompLayerByIndex(comp, li, &ly) != A_Err_NONE || !ly) continue;
        AEGP_LayerFlags fl = 0;
        lys->AEGP_GetLayerFlags(ly, &fl);
        if (!(fl & AEGP_LayerFlag_ADJUSTMENT_LAYER)) continue;
        std::vector<MincFxEntry> fx;
        MincEnumLayerEffects(bp, id, ly, &fx);
        for (size_t k = 0; k < fx.size(); ++k) {
            if (fx[k].name.compare(0, pre.size(), pre) != 0) continue;
            out.found = true;
            out.layer = ly;
            out.fxIndex1 = (int)k + 1;
            out.fxName = fx[k].name;
            for (size_t k2 = 0; k2 < fx.size(); ++k2)
                if (fx[k2].name.compare(0, 15, "minColor: look ") == 0) { out.lookIndex1 = (int)k2 + 1; out.lookName = fx[k2].name; break; }
            return out;
        }
    }
    return out;
}

struct UpResult { std::string action = "created", lookAction = "none", effName, error; bool disabledOther = false; };

static UpResult Upsert(SPBasicSuite *bp, AEGP_PluginID id, AEGP_CompH comp, AEGP_ItemH compItem,
                       const char *kind, const std::string &space, const std::string &pin) {
    UpResult r;
    bool isView = !strcmp(kind, "view");
    Acq<AEGP_ItemSuite9>  its(bp, kAEGPItemSuite,  kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12> cps(bp, kAEGPCompSuite,  kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9> lys(bp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    if (!its || !cps || !lys) { r.error = "suite acquire failed"; return r; }
    if (!MincSpaceInPin(space, pin)) {                       /* assertSpaceInPin (:1139-1143) */
        std::string pinBase = pin.substr(pin.find_last_of('/') == std::string::npos ? 0 : pin.find_last_of('/') + 1);
        std::string preset = MincPresetFromConfigBase(pinBase), pinned, current;
        bool behind = !preset.empty() && MincPinBehind(preset, pinBase, &pinned, &current);
        r.error = "'" + space + "' is not in this project's pinned config" +
                  (behind ? " (" + pinned + "; current is " + current + ") \xe2\x80\x94 Set Up / Migrate to the same preset updates the pin" : "");
        return r;
    }
    std::string newName = std::string("minColor: ") + kind + " " + space;
    MincUtilLayer ex = MincFindUtilityLayer(bp, id, comp, kind);
    AEGP_LayerH sol = nullptr;
    if (ex.found) {
        sol = ex.layer;
        r.action = "updated";
        lys->AEGP_SetLayerFlag(sol, AEGP_LayerFlag_VIDEO_ACTIVE, TRUE);
        A_long idx = 0;
        lys->AEGP_GetLayerIndex(sol, &idx);
        if (idx > 0) lys->AEGP_ReorderLayer(sol, 0);
        std::vector<MincFxEntry> fx;                          /* rename/retarget the transform */
        MincEnumLayerEffects(bp, id, sol, &fx);
        if (ex.fxIndex1 >= 1 && ex.fxIndex1 <= (int)fx.size()) {
            const std::string &mn = fx[ex.fxIndex1 - 1].match;
            MincRenameEffectAt(bp, id, sol, ex.fxIndex1, newName);
            if (mn == "ADBE OCIO Color Space Transform") {   /* panel's native fallback: retarget popups */
                Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5);
                AEGP_EffectRefH effH = nullptr;
                if (efs && efs->AEGP_GetLayerEffectByIndex(id, sol, ex.fxIndex1 - 1, &effH) == A_Err_NONE && effH) {
                    std::string e1, e2;
                    MincSetPopupByName(bp, id, effH, 1, "default", &e1);
                    MincSetPopupByName(bp, id, effH, 2, space, &e2);
                    efs->AEGP_DisposeEffect(effH);
                }
            }
        }
    } else {
        A_long w = 0, h = 0;
        its->AEGP_GetItemDimensions(compItem, &w, &h);
        A_Time dur = {0, 100};
        its->AEGP_GetItemDuration(compItem, &dur);
        AEGP_ColorVal black; black.alphaF = 1; black.redF = 0; black.greenF = 0; black.blueF = 0;
        A_UTF16Char nm16[64];
        MincU8ToU16(isView ? "minColor VIEW (guide)" : "minColor RENDER", nm16, 64);
        if (cps->AEGP_CreateSolidInComp(nm16, w, h, &black, comp, &dur, &sol) != A_Err_NONE || !sol) {
            r.error = "solid creation failed";
            return r;
        }
        lys->AEGP_SetLayerFlag(sol, AEGP_LayerFlag_ADJUSTMENT_LAYER, TRUE);
        lys->AEGP_SetLayerLabel(sol, isView ? 14 : 9);        /* panel label colors (:1157) */
        lys->AEGP_ReorderLayer(sol, 0);
        int endIdx = 0;
        AEGP_EffectRefH effH = MincApplyByMatchWithName(bp, id, sol,
                                   isView ? MINC_MATCH_VIEW : MINC_MATCH_RENDER, newName, &endIdx);
        if (!effH) { r.error = "variant apply failed"; return r; }
        { Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5); if (efs) efs->AEGP_DisposeEffect(effH); }
        /* look partner INHERIT (:1166-1176): a comp's look lives on whichever utility layer
           exists — a fresh sibling picks it up. Set/remove belongs to Apply Look.          */
        MincUtilLayer sib = MincFindUtilityLayer(bp, id, comp, isView ? "render" : "view");
        if (sib.found && sib.lookIndex1 > 0) {
            int lend = 0;
            AEGP_EffectRefH lf = MincApplyByMatchWithName(bp, id, sol, MINC_MATCH_LOOK, sib.lookName, &lend);
            if (lf) {
                { Acq<AEGP_EffectSuite5> efs2(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5); if (efs2) efs2->AEGP_DisposeEffect(lf); }
                if (lend != 1) MincMoveEffect(bp, id, sol, lend, 1);   /* look-then-display */
                r.lookAction = "inherited";
            }
        }
    }
    lys->AEGP_SetLayerFlag(sol, AEGP_LayerFlag_GUIDE_LAYER, isView ? TRUE : FALSE);   /* view never renders (:1160) */
    r.effName = newName;
    MincUtilLayer other = MincFindUtilityLayer(bp, id, comp, isView ? "render" : "view");
    if (other.found) {
        AEGP_LayerFlags ofl = 0;
        lys->AEGP_GetLayerFlags(other.layer, &ofl);
        if (ofl & AEGP_LayerFlag_VIDEO_ACTIVE) {
            lys->AEGP_SetLayerFlag(other.layer, AEGP_LayerFlag_VIDEO_ACTIVE, FALSE);
            r.disabledOther = true;                           /* activating one switches the other off (:1145) */
        }
    }
    return r;
}

static std::string LookOnLayer(SPBasicSuite *bp, AEGP_PluginID id, MincUtilLayer &u, const std::string &look) {
    if (look.empty()) {                                      /* remove */
        if (u.lookIndex1 > 0) return MincRemoveEffectAt(bp, id, u.layer, u.lookIndex1) ? "removed" : "remove failed";
        return "none";
    }
    std::string want = "minColor: look " + look;
    if (u.lookIndex1 == 0) {                                 /* add: look-then-display = index 1 */
        int end = 0;
        AEGP_EffectRefH lf = MincApplyByMatchWithName(bp, id, u.layer, MINC_MATCH_LOOK, want, &end);
        if (!lf) return "add failed";
        { Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5); if (efs) efs->AEGP_DisposeEffect(lf); }
        if (end != 1) MincMoveEffect(bp, id, u.layer, end, 1);
        return "added";
    }
    if (u.lookName != want) {
        if (!MincRenameEffectAt(bp, id, u.layer, u.lookIndex1, want)) return "rename failed";
        if (u.lookIndex1 != 1) MincMoveEffect(bp, id, u.layer, u.lookIndex1, 1);
        return "updated";
    }
    if (u.lookIndex1 != 1) { MincMoveEffect(bp, id, u.layer, u.lookIndex1, 1); return "reordered"; }
    return "none";
}

std::string MincApplyLook(SPBasicSuite *bp, AEGP_PluginID id, const std::string &look) {
    Acq<AEGP_ItemSuite9>  its(bp, kAEGPItemSuite,  kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12> cps(bp, kAEGPCompSuite,  kAEGPCompSuiteVersion12);
    Acq<AEGP_UtilitySuite6> uts(bp, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    if (!its || !cps) return "{ \"error\": \"suite acquire failed\" }\n";
    AEGP_ItemH active = nullptr;
    its->AEGP_GetActiveItem(&active);
    AEGP_ItemType aty = AEGP_ItemType_NONE;
    if (active) its->AEGP_GetItemType(active, &aty);
    if (!active || aty != AEGP_ItemType_COMP) return "{ \"error\": \"open a comp\" }\n";   /* :1218 verbatim */
    AEGP_CompH comp = nullptr;
    cps->AEGP_GetCompFromItem(active, &comp);
    if (!comp) return "{ \"error\": \"open a comp\" }\n";
    std::string vAct = "absent", rAct = "absent";
    if (uts) uts->AEGP_StartUndoGroup("minColor apply look");
    MincUtilLayer ur = MincFindUtilityLayer(bp, id, comp, "render");
    if (ur.found) rAct = LookOnLayer(bp, id, ur, look);
    MincUtilLayer uv = MincFindUtilityLayer(bp, id, comp, "view");
    if (uv.found) vAct = LookOnLayer(bp, id, uv, look);
    if (uts) uts->AEGP_EndUndoGroup();
    if (vAct == "absent" && rAct == "absent")
        return "{ \"error\": \"no minColor view/render layers in this comp \xe2\x80\x94 add one first\" }\n";   /* :1234 verbatim */
    MincSyncFromNames(bp, id);
    return "{ \"view\": " + UJ(vAct) + ", \"render\": " + UJ(rAct) + " }\n";
}

std::string MincEnsureUtilityLayers(SPBasicSuite *bp, AEGP_PluginID id,
                                    const std::string &viewSpace, const std::string &renderSpace) {
    Acq<AEGP_ItemSuite9>  its(bp, kAEGPItemSuite,  kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12> cps(bp, kAEGPCompSuite,  kAEGPCompSuiteVersion12);
    Acq<AEGP_UtilitySuite6> uts(bp, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    if (!its || !cps) return "{ \"error\": \"suite acquire failed\" }\n";
    AEGP_ItemH active = nullptr;
    its->AEGP_GetActiveItem(&active);
    AEGP_ItemType aty = AEGP_ItemType_NONE;
    if (active) its->AEGP_GetItemType(active, &aty);
    if (!active || aty != AEGP_ItemType_COMP) return "{ \"error\": \"open a comp\" }\n";   /* :1351 verbatim */
    AEGP_CompH comp = nullptr;
    cps->AEGP_GetCompFromItem(active, &comp);
    if (!comp) return "{ \"error\": \"open a comp\" }\n";
    MincAuthorityRefreshBp(bp, id);
    MincAuthoritySnapshot snap = {};
    MincAuthorityGet(&snap);
    std::string pin = snap.configPath;
    std::string view = viewSpace, render = renderSpace;
    if (view.empty() || render.empty()) {                    /* family defaults from the menus file */
        MincMenus menus;
        if (MincMenusGet(&menus)) {
            if (view.empty()) view = menus.defaultView;
            if (render.empty()) render = menus.defaultRender;
        }
    }
    if (view.empty() || render.empty()) return "{ \"error\": \"no spaces given and no plugin-menus defaults\" }\n";
    if (uts) uts->AEGP_StartUndoGroup("minColor utility layers");
    UpResult rr = Upsert(bp, id, comp, active, "render", render, pin);   /* render first, */
    UpResult rv = Upsert(bp, id, comp, active, "view",   view,   pin);   /* view last -> view on, render off (:1356) */
    if (uts) uts->AEGP_EndUndoGroup();
    MincSyncFromNames(bp, id);
    if (!rr.error.empty()) return "{ \"error\": " + UJ(rr.error) + " }\n";
    if (!rv.error.empty()) return "{ \"error\": " + UJ(rv.error) + " }\n";
    /* ensureUtilityLayers return shape (:1358-1360) + look/disable detail */
    return "{ \"view\": " + UJ(view) + ", \"viewAction\": " + UJ(rv.action) +
           ", \"render\": " + UJ(render) + ", \"renderAction\": " + UJ(rr.action) +
           ", \"viewLook\": " + UJ(rv.lookAction) + ", \"renderLook\": " + UJ(rr.lookAction) + " }\n";
}

/* applyRenderPreset port (:1347-1354): recipe {view, render, look?} -> BOTH utility layers
   (view ends enabled), recipe look set or — absent — REMOVED (pr.look || null semantics).
   Recipes from settings/render-presets.json (user-editable, seeded by the installer).     */
#include "MincJson.h"
#include "MincSettings.h"
std::string MincApplyRenderPreset(SPBasicSuite *bp, AEGP_PluginID id, const std::string &name) {
    if (name.empty()) return "{ \"error\": \"no preset name given\" }\n";
    MincJsonPtr j = MincJsonParseFile(MincSettingsDir() + "/render-presets.json");
    MincJsonPtr presets = j ? j->get("presets") : nullptr;
    MincJsonPtr pr = presets ? presets->get(name) : nullptr;
    if (!pr) return "{ \"error\": " + UJ("unknown render preset '" + name + "'") + " }\n";   /* :1349 verbatim */
    std::string view = pr->str("view"), render = pr->str("render"), look = pr->str("look");
    std::string uj = MincEnsureUtilityLayers(bp, id, view, render);
    if (uj.find("\"error\"") != std::string::npos) return uj;
    std::string lj = MincApplyLook(bp, id, look);          /* "" = remove (pr.look || null) */
    if (lj.find("\"error\"") != std::string::npos) return lj;
    return "{ \"preset\": " + UJ(name) + ", \"view\": " + UJ(view) + ", \"render\": " + UJ(render) +
           ", \"look\": " + UJ(look.empty() ? "(none)" : look) + " }\n";
}
