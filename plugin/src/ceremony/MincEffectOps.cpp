#include "MincEffectOps.h"
#include <cstring>

/* parade-group stream ref for a layer (the walk's pattern, factored) */
static AEGP_StreamRefH ParadeRef(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH) {
    Acq<AEGP_DynamicStreamSuite4> dss(bp, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4);
    Acq<AEGP_StreamSuite6>        sts(bp, kAEGPStreamSuite,        kAEGPStreamSuiteVersion6);
    if (!dss || !sts) return nullptr;
    AEGP_StreamRefH layerRef = nullptr, fxGroup = nullptr;
    if (dss->AEGP_GetNewStreamRefForLayer(id, layerH, &layerRef) != A_Err_NONE || !layerRef) return nullptr;
    A_long nkids = 0;
    if (dss->AEGP_GetNumStreamsInGroup(layerRef, &nkids) == A_Err_NONE) {
        for (A_long ki = 0; ki < nkids && !fxGroup; ++ki) {
            AEGP_StreamRefH kid = nullptr;
            if (dss->AEGP_GetNewStreamRefByIndex(id, layerRef, ki, &kid) != A_Err_NONE || !kid) continue;
            A_char km[AEGP_MAX_STREAM_MATCH_NAME_SIZE] = "";
            dss->AEGP_GetMatchName(kid, km);
            if (!strcmp(km, "ADBE Effect Parade")) fxGroup = kid;
            else sts->AEGP_DisposeStream(kid);
        }
    }
    sts->AEGP_DisposeStream(layerRef);
    return fxGroup;                                          /* caller disposes */
}

bool MincEnumLayerEffects(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH,
                          std::vector<MincFxEntry> *out) {
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_DynamicStreamSuite4> dss(bp, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4);
    Acq<AEGP_StreamSuite6>        sts(bp, kAEGPStreamSuite,        kAEGPStreamSuiteVersion6);
    if (!dss || !sts) return false;
    AEGP_StreamRefH fxGroup = ParadeRef(bp, id, layerH);
    if (!fxGroup) return true;                               /* no parade (camera/light) = no effects */
    A_long nfx = 0;
    dss->AEGP_GetNumStreamsInGroup(fxGroup, &nfx);
    for (A_long fi = 0; fi < nfx; ++fi) {
        AEGP_StreamRefH fxRef = nullptr;
        if (dss->AEGP_GetNewStreamRefByIndex(id, fxGroup, fi, &fxRef) != A_Err_NONE || !fxRef) continue;
        MincFxEntry e;
        A_char match[AEGP_MAX_STREAM_MATCH_NAME_SIZE] = "";
        dss->AEGP_GetMatchName(fxRef, match);
        e.match = match;
        AEGP_MemHandle nameH = nullptr;
        char n8[512] = "";
        if (sts->AEGP_GetStreamName(id, fxRef, FALSE, &nameH) == A_Err_NONE)
            MincUtf16HandleToUtf8(suites, nameH, n8, sizeof(n8));
        e.name = n8;
        out->push_back(e);
        sts->AEGP_DisposeStream(fxRef);
    }
    sts->AEGP_DisposeStream(fxGroup);
    return true;
}

bool MincRenameEffectAt(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH,
                        int index1, const std::string &newName) {
    Acq<AEGP_DynamicStreamSuite4> dss(bp, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4);
    Acq<AEGP_StreamSuite6>        sts(bp, kAEGPStreamSuite,        kAEGPStreamSuiteVersion6);
    if (!dss || !sts) return false;
    AEGP_StreamRefH fxGroup = ParadeRef(bp, id, layerH);
    if (!fxGroup) return false;
    bool ok = false;
    AEGP_StreamRefH fxRef = nullptr;
    if (dss->AEGP_GetNewStreamRefByIndex(id, fxGroup, index1 - 1, &fxRef) == A_Err_NONE && fxRef) {
        A_UTF16Char u16[512];
        MincU8ToU16(newName.c_str(), u16, 512);
        ok = (dss->AEGP_SetStreamName(fxRef, u16) == A_Err_NONE);
        sts->AEGP_DisposeStream(fxRef);
    }
    sts->AEGP_DisposeStream(fxGroup);
    return ok;
}

bool MincApplyMincWithName(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH,
                           const std::string &grammarName, int targetIndex1) {
    Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5);
    if (!efs) return false;
    AEGP_InstalledEffectKey key = MincInstalledKey(bp, id);
    if (!key) { MincLog("apply: MINC CST not installed"); return false; }
    AEGP_EffectRefH effH = nullptr;
    if (efs->AEGP_ApplyEffect(id, layerH, key, &effH) != A_Err_NONE || !effH) return false;
    std::vector<MincFxEntry> fx;
    MincEnumLayerEffects(bp, id, layerH, &fx);               /* new effect is the LAST parade child */
    int newIdx = (int)fx.size();
    bool ok = MincRenameEffectAt(bp, id, layerH, newIdx, grammarName);
    if (ok && targetIndex1 > 0 && targetIndex1 != newIdx)
        efs->AEGP_ReorderEffect(effH, targetIndex1 - 1);     /* AEGP effect index is 0-based */
    efs->AEGP_DisposeEffect(effH);
    return ok;
}

bool MincRemoveEffectAt(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH, int index1) {
    Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5);
    if (!efs) return false;
    AEGP_EffectRefH effH = nullptr;
    if (efs->AEGP_GetLayerEffectByIndex(id, layerH, index1 - 1, &effH) != A_Err_NONE || !effH) return false;
    bool ok = (efs->AEGP_DeleteLayerEffect(effH) == A_Err_NONE);
    efs->AEGP_DisposeEffect(effH);
    return ok;
}

bool MincMoveEffect(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH, int fromIndex1, int toIndex1) {
    Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5);
    if (!efs) return false;
    AEGP_EffectRefH effH = nullptr;
    if (efs->AEGP_GetLayerEffectByIndex(id, layerH, fromIndex1 - 1, &effH) != A_Err_NONE || !effH) return false;
    bool ok = (efs->AEGP_ReorderEffect(effH, toIndex1 - 1) == A_Err_NONE);
    efs->AEGP_DisposeEffect(effH);
    return ok;
}

bool MincLayerLocked(SPBasicSuite *bp, AEGP_LayerH layerH) {
    Acq<AEGP_LayerSuite9> lys(bp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    if (!lys) return false;
    AEGP_LayerFlags fl = 0;
    lys->AEGP_GetLayerFlags(layerH, &fl);
    return (fl & AEGP_LayerFlag_LOCKED) != 0;
}
void MincSetLayerLocked(SPBasicSuite *bp, AEGP_LayerH layerH, bool locked) {
    Acq<AEGP_LayerSuite9> lys(bp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    if (lys) lys->AEGP_SetLayerFlag(layerH, AEGP_LayerFlag_LOCKED, locked ? TRUE : FALSE);
}

AEGP_InstalledEffectKey MincInstalledKey(SPBasicSuite *bp, AEGP_PluginID id) {
    static AEGP_InstalledEffectKey cached = 0;
    static bool scanned = false;
    if (scanned) return cached;
    scanned = true;
    Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5);
    if (!efs) { MincLog("effect-key scan: suite acquire failed"); return 0; }
    double t0 = MincNowMs();
    int n = 0;
    AEGP_InstalledEffectKey k = AEGP_InstalledEffectKey_NONE, next = AEGP_InstalledEffectKey_NONE;
    while (efs->AEGP_GetNextInstalledEffect(k, &next) == A_Err_NONE && next != AEGP_InstalledEffectKey_NONE) {
        ++n;
        A_char mn[64] = "";
        if (efs->AEGP_GetEffectMatchName(next, mn) == A_Err_NONE && !strcmp(mn, MINC_MATCH_NAME)) cached = next;
        k = next;
    }
    double dt = MincNowMs() - t0;
    /* the iterate-vs-keyed-lookup answer, on the record (plan: M1 step 2) */
    MincLog("effect-key scan: %d installed effects in %.2f ms; MINC CST key=%d", n, dt, (int)cached);
    (void)id;
    return cached;
}
