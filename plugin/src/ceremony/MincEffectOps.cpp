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
    /* M3 authoring swap: interpret/migrate author the XFORM VARIANT (verb = match name),
       no longer legacy MINC CST — grammar input/contain names are XFORM-legal. §34
       ordering: nothing is written through the ref here, so dispose BEFORE the move. */
    int newIdx = 0;
    AEGP_EffectRefH effH = MincApplyByMatchWithName(bp, id, layerH, MINC_MATCH_XFORM, grammarName, &newIdx);
    if (!effH) return false;
    Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5);
    if (efs) efs->AEGP_DisposeEffect(effH);
    if (targetIndex1 > 0 && targetIndex1 != newIdx)
        MincMoveEffect(bp, id, layerH, newIdx, targetIndex1);
    return true;
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

#include <map>
#include <vector>
static AEGP_InstalledEffectKey KeyByMatch(SPBasicSuite *bp, const char *matchName) {
    static std::map<std::string, AEGP_InstalledEffectKey> cache;
    auto it = cache.find(matchName);
    if (it != cache.end()) return it->second;
    Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5);
    AEGP_InstalledEffectKey found = 0;
    if (efs) {
        AEGP_InstalledEffectKey k = AEGP_InstalledEffectKey_NONE, nx = AEGP_InstalledEffectKey_NONE;
        while (efs->AEGP_GetNextInstalledEffect(k, &nx) == A_Err_NONE && nx != AEGP_InstalledEffectKey_NONE) {
            A_char mn[64] = "";
            if (efs->AEGP_GetEffectMatchName(nx, mn) == A_Err_NONE && !strcmp(mn, matchName)) { found = nx; break; }
            k = nx;
        }
    }
    cache[matchName] = found;
    return found;
}

AEGP_EffectRefH MincApplyByMatchWithName(SPBasicSuite *bp, AEGP_PluginID id, AEGP_LayerH layerH,
                                         const char *matchName, const std::string &dispName,
                                         int *appliedIndex1) {
    /* The effect is applied at the parade END and LEFT there — the returned ref resolves
       by parade position, so a reorder here would silently rebind it to whatever effect
       shifted into that slot (M2 crash of record: popup writes landed on a MINC arb stream
       → SIGSEGV in the arb copy callback; Sentry dmp + our handler both show it). Callers
       finish every stream write through this ref FIRST, dispose it, then position the
       effect with MincMoveEffect.                                                        */
    Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5);
    if (!efs) return nullptr;
    AEGP_InstalledEffectKey key = KeyByMatch(bp, matchName);
    if (!key) { MincLog("apply: '%s' not installed", matchName); return nullptr; }
    AEGP_EffectRefH effH = nullptr;
    if (efs->AEGP_ApplyEffect(id, layerH, key, &effH) != A_Err_NONE || !effH) return nullptr;
    std::vector<MincFxEntry> fx;
    MincEnumLayerEffects(bp, id, layerH, &fx);
    int newIdx = (int)fx.size();
    if (!MincRenameEffectAt(bp, id, layerH, newIdx, dispName)) { efs->AEGP_DisposeEffect(effH); return nullptr; }
    if (appliedIndex1) *appliedIndex1 = newIdx;
    return effH;                                          /* caller disposes */
}

bool MincSetPopupByName(SPBasicSuite *bp, AEGP_PluginID id, AEGP_EffectRefH effH,
                        int paramIdx, const std::string &wanted, std::string *errOut) {
    Acq<AEGP_EffectSuite5> efs(bp, kAEGPEffectSuite, kAEGPEffectSuiteVersion5);
    Acq<AEGP_StreamSuite6> sts(bp, kAEGPStreamSuite, kAEGPStreamSuiteVersion6);
    if (!efs || !sts) { if (errOut) *errOut = "suites"; return false; }
    PF_ParamType pt = PF_Param_RESERVED;
    PF_ParamDefUnion u;
    memset(&u, 0, sizeof(u));
    if (efs->AEGP_GetEffectParamUnionByIndex(id, effH, paramIdx, &pt, &u) != A_Err_NONE ||
        pt != PF_Param_POPUP || !u.pd.u.namesptr) {
        if (errOut) *errOut = "popup enumerate failed";
        return false;
    }
    std::vector<std::string> list;
    {
        const char *p = u.pd.u.namesptr;
        std::string cur;
        for (; *p; ++p) { if (*p == '|') { list.push_back(cur); cur.clear(); } else cur += *p; }
        list.push_back(cur);
    }
    int idx = -1;
    for (size_t i = 0; i < list.size() && idx < 0; ++i) if (list[i] == wanted) idx = (int)i;
    if (idx < 0)                                          /* role entries: "wanted: <name>" */
        for (size_t i = 0; i < list.size() && idx < 0; ++i)
            if (list[i].compare(0, wanted.size() + 2, wanted + ": ") == 0) idx = (int)i;
    if (idx < 0)                                          /* suffix "/wanted" or ": wanted" */
        for (size_t i = 0; i < list.size() && idx < 0; ++i) {
            const std::string &s = list[i];
            if (s.size() > wanted.size() + 1 && s.compare(s.size() - wanted.size() - 1, std::string::npos, "/" + wanted) == 0) idx = (int)i;
            else if (s.size() > wanted.size() + 2 && s.compare(s.size() - wanted.size() - 2, std::string::npos, ": " + wanted) == 0) idx = (int)i;
        }
    if (idx < 0) {
        if (errOut) *errOut = "'" + wanted + "' not found among " + std::to_string(list.size()) + " entries";
        return false;
    }
    AEGP_StreamRefH s1 = nullptr;
    if (sts->AEGP_GetNewEffectStreamByIndex(id, effH, paramIdx, &s1) != A_Err_NONE || !s1) {
        if (errOut) *errOut = "stream";
        return false;
    }
    AEGP_StreamValue2 v;
    memset(&v, 0, sizeof(v));
    v.streamH = s1;
    v.val.one_d = idx + 1;                                /* 1-based, Probe H verified */
    A_Err se = sts->AEGP_SetStreamValue(id, s1, &v);
    sts->AEGP_DisposeStream(s1);
    if (se != A_Err_NONE) { if (errOut) *errOut = "SetStreamValue err"; return false; }
    return true;
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
        if (efs->AEGP_GetEffectMatchName(next, mn) == A_Err_NONE && !strcmp(mn, MINC_MATCH_XFORM)) cached = next;   /* M3 step 8: legacy unregistered on mac — scan the live XFORM */
        k = next;
    }
    double dt = MincNowMs() - t0;
    /* the iterate-vs-keyed-lookup answer, on the record (plan: M1 step 2) */
    MincLog("effect-key scan: %d installed effects in %.2f ms; MINC XFORM key=%d", n, dt, (int)cached);
    (void)id;
    return cached;
}
