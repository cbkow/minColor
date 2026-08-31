/* Badge-edit transaction (M2 step 5) — the click-time native menu's write path. Rename the
   effect stream via the param-stream→parent trick (the only route to the effect stream from
   inside the effect), ALWAYS eager-mint a fresh instance id (the edit IS the divergence
   point: an un-synced copy sharing the old id must never see this instance's new state
   through the registry), targeted CallGeneric so the clicked instance is correct
   immediately — then touch the walk marker: the AEGP owns every full walk (P2), which also
   repairs other duplicates and refreshes passports within ~1s.                            */
#include "MinColorCST.h"
#include <cstring>
#include <cstdio>

bool MincApplyEdit(PF_InData *in_data, int verbI, const char *space) {
    MincVerb verb = (MincVerb)verbI;
    SPBasicSuite *bp = in_data->pica_basicP;
    if (!bp || !MincEffectRegistered() || !space || !space[0]) return false;
    AEGP_PluginID aegpId = MincEffectAegpId();
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_PFInterfaceSuite1>   pfi(bp, kAEGPPFInterfaceSuite,   kAEGPPFInterfaceSuiteVersion1);
    Acq<AEGP_EffectSuite5>        efs(bp, kAEGPEffectSuite,        kAEGPEffectSuiteVersion5);
    Acq<AEGP_StreamSuite6>        sts(bp, kAEGPStreamSuite,        kAEGPStreamSuiteVersion6);
    Acq<AEGP_DynamicStreamSuite4> dss(bp, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4);
    Acq<AEGP_UtilitySuite6>       uts(bp, kAEGPUtilitySuite,       kAEGPUtilitySuiteVersion6);
    if (!pfi || !efs || !sts || !dss) { MincLog("edit: suite acquire failed"); return false; }
    AEGP_EffectRefH effH = nullptr;
    if (pfi->AEGP_GetNewEffectForEffect(aegpId, in_data->effect_ref, &effH) != A_Err_NONE || !effH) return false;
    bool ok = false;
    char nn[300] = "";
    AEGP_StreamRefH paramS = nullptr, fxS = nullptr;
    if (sts->AEGP_GetNewEffectStreamByIndex(aegpId, effH, MINC_ARB, &paramS) == A_Err_NONE && paramS) {
        if (dss->AEGP_GetNewParentStreamRef(aegpId, paramS, &fxS) == A_Err_NONE && fxS) {
            char curName[512] = "";
            {   AEGP_MemHandle nh = nullptr;
                if (sts->AEGP_GetStreamName(aegpId, fxS, FALSE, &nh) == A_Err_NONE)
                    MincUtf16HandleToUtf8(suites, nh, curName, sizeof(curName));
            }
            if (verb == MINC_VERB_XFORM) {                          /* keep the contain spelling if that's what it is */
                if (!strncmp(curName, "minColor: contain ", 18)) snprintf(nn, sizeof(nn), "minColor: contain %s", space);
                else snprintf(nn, sizeof(nn), "minColor: %s \xe2\x86\x92 working", space);
            } else {
                snprintf(nn, sizeof(nn), "minColor: %s %s",
                         verb == MINC_VERB_VIEW ? "view" : (verb == MINC_VERB_RENDER ? "render" : "look"), space);
            }
            A_UTF16Char u16[300]; MincU8ToU16(nn, u16, 300);
            if (uts) uts->AEGP_StartUndoGroup("minColor: set space");
            A_Err re = dss->AEGP_SetStreamName(fxS, u16);
            MinColorArb want;
            if (re == A_Err_NONE && MincParseGrammarVerb(verb, nn, &want)) {
                MincAuthorityRefresh(in_data);                       /* passport source = current healthy authority */
                MincAuthoritySnapshot snap = {}; MincAuthorityGet(&snap);
                MincSyncPayload pay; memset(&pay, 0, sizeof(pay));
                pay.magic = MINC_ARB_MAGIC; pay.arb = want; pay.payVersion = 3;
                pay.newId = MincMintInstanceId();                    /* ALWAYS — see header comment */
                if (snap.ocioOn && snap.configPath[0] && snap.workingSpace[0]) {
                    const char *b = snap.configPath;
                    for (const char *p = snap.configPath; *p; ++p) if (*p == '/' || *p == '\\') b = p + 1;
                    if (b[0] && !strstr(b, "..") && strlen(b) < sizeof(pay.configBase)) {
                        strncpy(pay.configBase, b, sizeof(pay.configBase) - 1);
                        strncpy(pay.passportWorking, snap.workingSpace, sizeof(pay.passportWorking) - 1);
                    }
                }
                A_Time tg = {0, 100};
                ok = (efs->AEGP_EffectCallGeneric(aegpId, effH, &tg, PF_Cmd_COMPLETELY_GENERAL, &pay) == A_Err_NONE);
                if (ok) {
                    /* the CallGeneric arb write never surfaces as a param EVENT, so the ECW badge
                       kept drawing the old space until some other param changed (step-5 finding;
                       PF_OutFlag_REFRESH_UI from the event was not enough). Bump Sync Serial —
                       the panel-proven repaint path — through THIS fresh ref (no reorder between
                       acquire and write: §34 safe). Amends the audit's "serial never bumps on
                       edit" note: the arb covers cache identity, the serial write is for the UI. */
                    AEGP_StreamRefH ser = nullptr;
                    if (sts->AEGP_GetNewEffectStreamByIndex(aegpId, effH, MINC_SERIAL, &ser) == A_Err_NONE && ser) {
                        AEGP_StreamValue2 v;
                        memset(&v, 0, sizeof(v));
                        v.streamH = ser;
                        v.val.one_d = (double)(((long)MincNowMs()) % 900000);
                        sts->AEGP_SetStreamValue(aegpId, ser, &v);
                        sts->AEGP_DisposeStream(ser);
                    }
                }
            }
            if (uts) uts->AEGP_EndUndoGroup();
            sts->AEGP_DisposeStream(fxS);
        }
        sts->AEGP_DisposeStream(paramS);
    }
    efs->AEGP_DisposeEffect(effH);
    MincLog("edit: -> '%s' %s", nn, ok ? "ok" : "FAILED");
    if (ok) MincTouchWalkMarker("sync");                             /* AEGP walks (single-walker, P2) */
    return ok;
}
