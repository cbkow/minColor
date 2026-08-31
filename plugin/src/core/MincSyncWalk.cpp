/* ---------------- "minColor: Sync From Names" — the walk ----------------
   The panel names MINC CST instances ("minColor: X \xe2\x86\x92 working" / "minColor: view X" /
   "minColor: render X") and invokes the menu command; we parse every instance's display name
   and write its arb param. AEGP CAN set arbitrary streams; ExtendScript cannot.
   Location-independent: acts via AEGP suites + AEGP_EffectCallGeneric, which dispatches into
   the EFFECT binary's HandleGeneric/registry regardless of which bundle runs the walk.       */
#include "MincCore.h"
#include "MincMenus.h"
#include <set>
#include <cstring>
#include <cstdio>

/* christening gate: ONLY AE's untouched default display name — the effect name ("minColor
   View") plus an optional " <digits>" copy suffix. A user-typed name is never overwritten. */
static bool IsDefaultEffectName(MincVerb verb, const char *n) {
    const char *base = (verb == MINC_VERB_VIEW) ? MINC_NAME_VIEW
                     : (verb == MINC_VERB_RENDER) ? MINC_NAME_RENDER : nullptr;
    if (!base) return false;
    size_t bl = strlen(base);
    if (strncmp(n, base, bl) != 0) return false;
    const char *p = n + bl;
    if (!*p) return true;
    if (*p != ' ') return false;
    for (++p; *p; ++p) if (*p < '0' || *p > '9') return false;
    return *(n + bl + 1) != 0;
}

static bool ParseGrammar(MincVerb verb, const char *utf8, MinColorArb *out) {
    const char *PRE = "minColor: ";
    if (strncmp(utf8, PRE, 10) != 0) return false;
    const char *rest = utf8 + 10;
    memset(out, 0, sizeof(*out));
    out->magic = MINC_ARB_MAGIC; out->version = MINC_ARB_VERSION;
    if (verb == MINC_VERB_LEGACY) {
        /* the pre-2.0 grammar, verbatim: the NAME carries the verb (scenario 12 contract) */
        if (!strncmp(rest, "look ", 5))   { out->direction = MINC_DIR_LOOK;         snprintf(out->space, MINC_SPACE_LEN, "%s", rest + 5); return true; }
        if (!strncmp(rest, "contain ", 8)) { out->direction = MINC_DIR_TO_WORKING;   snprintf(out->space, MINC_SPACE_LEN, "%s", rest + 8); return true; }   /* boundary: comp output IS media in X */
        if (!strncmp(rest, "view ", 5))   { out->direction = MINC_DIR_FROM_WORKING; snprintf(out->space, MINC_SPACE_LEN, "%s", rest + 5); return true; }
        if (!strncmp(rest, "render ", 7)) { out->direction = MINC_DIR_FROM_WORKING; snprintf(out->space, MINC_SPACE_LEN, "%s", rest + 7); return true; }
        const char *arrow = strstr(rest, " \xe2\x86\x92 working");    /* " -> working", real arrow */
        if (!arrow) arrow = strstr(rest, " -> working");
        if (!arrow) return false;
        out->direction = MINC_DIR_TO_WORKING;
        size_t n = (size_t)(arrow - rest); if (n >= MINC_SPACE_LEN) n = MINC_SPACE_LEN - 1;
        memcpy(out->space, rest, n);
        return true;
    }
    /* variant: the MATCH NAME is the verb authority, the name carries the space. Accepted:
       "minColor: <verb> <space>" (redundant token must AGREE), bare "minColor: <space>",
       and on XFORM only "<space> → working" and "contain <space>". A contradicting verb
       token or a misplaced arrow is NEVER silently reinterpreted (-> unparsed -> warn),
       and an empty space stays unset.                                                    */
    static const struct { const char *tok; size_t n; MincVerb v; } TOKS[] = {
        { "look ",    5, MINC_VERB_LOOK   },
        { "contain ", 8, MINC_VERB_XFORM  },
        { "view ",    5, MINC_VERB_VIEW   },
        { "render ",  7, MINC_VERB_RENDER },
    };
    for (size_t i = 0; i < sizeof(TOKS) / sizeof(TOKS[0]); ++i) {
        if (!strncmp(rest, TOKS[i].tok, TOKS[i].n)) {
            if (TOKS[i].v != verb) return false;                     /* verb contradiction */
            rest += TOKS[i].n;
            break;
        }
    }
    const char *arrow = strstr(rest, " \xe2\x86\x92 working");
    if (!arrow) arrow = strstr(rest, " -> working");
    if (arrow) {
        if (verb != MINC_VERB_XFORM) return false;                   /* arrow is XFORM-only */
        size_t n = (size_t)(arrow - rest); if (n >= MINC_SPACE_LEN) n = MINC_SPACE_LEN - 1;
        memcpy(out->space, rest, n);
    } else snprintf(out->space, MINC_SPACE_LEN, "%s", rest);
    out->direction = (verb == MINC_VERB_LOOK)  ? MINC_DIR_LOOK
                   : (verb == MINC_VERB_XFORM) ? MINC_DIR_TO_WORKING
                                               : MINC_DIR_FROM_WORKING;
    return out->space[0] != 0;
}

static int SyncOnce(SPBasicSuite *bp, AEGP_PluginID aegpId, bool christen) {
    AEGP_SuiteHandler suites(bp);
    Acq<AEGP_ProjSuite6>          pjs(bp, kAEGPProjSuite,          kAEGPProjSuiteVersion6);
    Acq<AEGP_ItemSuite9>          its(bp, kAEGPItemSuite,          kAEGPItemSuiteVersion9);
    Acq<AEGP_CompSuite12>         cps(bp, kAEGPCompSuite,          kAEGPCompSuiteVersion12);
    Acq<AEGP_LayerSuite9>         lys(bp, kAEGPLayerSuite,         kAEGPLayerSuiteVersion9);
    Acq<AEGP_DynamicStreamSuite4> dss(bp, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4);
    Acq<AEGP_StreamSuite6>        sts(bp, kAEGPStreamSuite,        kAEGPStreamSuiteVersion6);
    Acq<AEGP_EffectSuite5>        efs(bp, kAEGPEffectSuite,        kAEGPEffectSuiteVersion5);
    if (!pjs || !its || !cps || !lys || !dss || !sts || !efs) { MincLog("sync: suite acquire failed"); return 0; }
    /* passport source: the CURRENT healthy authority. Written into every synced instance so the
       .aep carries the content-addressed config basename + working space across platforms. */
    MincAuthoritySnapshot snap = {}; MincAuthorityGet(&snap);
    bool authHealthy = snap.ocioOn && snap.configPath[0] && snap.workingSpace[0];
    char passBase[MINC_CONFIGBASE_LEN] = "";
    if (authHealthy) {
        const char *b = snap.configPath;
        for (const char *p = snap.configPath; *p; ++p) if (*p == '/' || *p == '\\') b = p + 1;
        if (b[0] && !strstr(b, "..") && strlen(b) < sizeof(passBase)) strncpy(passBase, b, sizeof(passBase) - 1);
    }
    int seen = 0, wrote = 0, badname = 0, reminted = 0, placeholders = 0, christened = 0;
    MincMenus menus;
    bool haveMenus = christen && MincMenusGet(&menus);   /* no menus file -> no christening, ever */
    Acq<AEGP_UtilitySuite6> uts(bp, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    std::set<uint32_t> idsThisWalk;                 /* duplicate ids (cross-session mints) get re-minted here */
    AEGP_ProjectH projH = nullptr;
    if (pjs->AEGP_GetProjectByIndex(0, &projH) != A_Err_NONE || !projH) return 0;
    AEGP_ItemH itemH = nullptr;
    its->AEGP_GetFirstProjItem(projH, &itemH);
    while (itemH) {
        AEGP_ItemType ty = AEGP_ItemType_NONE;
        its->AEGP_GetItemType(itemH, &ty);
        if (ty == AEGP_ItemType_COMP) {
            AEGP_CompH compH = nullptr;
            if (cps->AEGP_GetCompFromItem(itemH, &compH) == A_Err_NONE && compH) {
                A_long nL = 0; lys->AEGP_GetCompNumLayers(compH, &nL);
                for (A_long li = 0; li < nL; ++li) {
                    AEGP_LayerH layerH = nullptr;
                    if (lys->AEGP_GetCompLayerByIndex(compH, li, &layerH) != A_Err_NONE || !layerH) continue;
                    AEGP_StreamRefH layerRef = nullptr, fxGroup = nullptr;
                    if (dss->AEGP_GetNewStreamRefForLayer(aegpId, layerH, &layerRef) != A_Err_NONE || !layerRef) continue;
                    {   /* find the effects group by ITERATION — ByMatchname raises an internal
                           verification failure on layers without one (cameras, lights) */
                        A_long nkids = 0;
                        if (dss->AEGP_GetNumStreamsInGroup(layerRef, &nkids) == A_Err_NONE) {
                            for (A_long ki = 0; ki < nkids && !fxGroup; ++ki) {
                                AEGP_StreamRefH kid = nullptr;
                                if (dss->AEGP_GetNewStreamRefByIndex(aegpId, layerRef, ki, &kid) != A_Err_NONE || !kid) continue;
                                A_char km[AEGP_MAX_STREAM_MATCH_NAME_SIZE] = "";
                                dss->AEGP_GetMatchName(kid, km);
                                if (!strcmp(km, "ADBE Effect Parade")) fxGroup = kid;
                                else sts->AEGP_DisposeStream(kid);
                            }
                        }
                    }
                    if (fxGroup) {
                        A_long nfx = 0; dss->AEGP_GetNumStreamsInGroup(fxGroup, &nfx);
                        for (A_long fi = 0; fi < nfx; ++fi) {
                            AEGP_StreamRefH fxRef = nullptr;
                            if (dss->AEGP_GetNewStreamRefByIndex(aegpId, fxGroup, fi, &fxRef) != A_Err_NONE || !fxRef) continue;
                            A_char match[AEGP_MAX_STREAM_MATCH_NAME_SIZE] = "";
                            dss->AEGP_GetMatchName(fxRef, match);
                            MincVerb fxVerb = MINC_VERB_LEGACY;
                            if (MincMatchVerb(match, &fxVerb)) {
                                ++seen;
                                {   /* legacy re-hide: pre-1.2 instances persist a VISIBLE Sync Serial row
                                       (AE stores ui_flags per instance). AEGP_DynStreamFlag_HIDDEN is the
                                       one flag legal to set non-undoably; only touch it when not set. */
                                    A_long nkids2 = 0;
                                    if (dss->AEGP_GetNumStreamsInGroup(fxRef, &nkids2) == A_Err_NONE) {
                                        for (A_long ci = 0; ci < nkids2; ++ci) {
                                            AEGP_StreamRefH kid2 = nullptr;
                                            if (dss->AEGP_GetNewStreamRefByIndex(aegpId, fxRef, ci, &kid2) != A_Err_NONE || !kid2) continue;
                                            AEGP_MemHandle knameH = nullptr; char kname[128] = "";
                                            if (sts->AEGP_GetStreamName(aegpId, kid2, FALSE, &knameH) == A_Err_NONE)
                                                MincUtf16HandleToUtf8(suites, knameH, kname, sizeof(kname));
                                            if (!strcmp(kname, "Sync Serial")) {
                                                AEGP_DynStreamFlags kf = 0;
                                                if (dss->AEGP_GetDynamicStreamFlags(kid2, &kf) == A_Err_NONE &&
                                                    !(kf & AEGP_DynStreamFlag_HIDDEN))
                                                    dss->AEGP_SetDynamicStreamFlag(kid2, AEGP_DynStreamFlag_HIDDEN, FALSE, TRUE);
                                            }
                                            sts->AEGP_DisposeStream(kid2);
                                            if (kname[0] && !strcmp(kname, "Sync Serial")) break;
                                        }
                                    }
                                }
                                AEGP_MemHandle nameH = nullptr;
                                char name8[512] = "";
                                if (sts->AEGP_GetStreamName(aegpId, fxRef, FALSE, &nameH) == A_Err_NONE)
                                    MincUtf16HandleToUtf8(suites, nameH, name8, sizeof(name8));
                                MinColorArb want;
                                bool parsed = ParseGrammar(fxVerb, name8, &want);
                                if (!parsed && haveMenus &&
                                    (fxVerb == MINC_VERB_VIEW || fxVerb == MINC_VERB_RENDER) &&
                                    IsDefaultEffectName(fxVerb, name8)) {
                                    /* christening: a fresh drop still wearing AE's default name gets the
                                       family default — name-first, undoable, XFORM/LOOK never christened */
                                    char cname[MINC_SPACE_LEN + 32];
                                    snprintf(cname, sizeof(cname), "minColor: %s %s",
                                             fxVerb == MINC_VERB_VIEW ? "view" : "render",
                                             fxVerb == MINC_VERB_VIEW ? menus.defaultView : menus.defaultRender);
                                    A_UTF16Char u16[MINC_SPACE_LEN + 32];
                                    MincU8ToU16(cname, u16, MINC_SPACE_LEN + 32);
                                    if (uts) uts->AEGP_StartUndoGroup("minColor: name new effect");
                                    A_Err re = dss->AEGP_SetStreamName(fxRef, u16);
                                    if (uts) uts->AEGP_EndUndoGroup();
                                    if (re == A_Err_NONE) {
                                        ++christened;
                                        snprintf(name8, sizeof(name8), "%s", cname);
                                        parsed = ParseGrammar(fxVerb, name8, &want);
                                        MincLog("sync: christened '%s'", cname);
                                    }
                                }
                                if (parsed) {
                                    /* write via the EFFECT's param stream (Projector-sample pattern):
                                       parade child index == effect index, verified by matchname */
                                    AEGP_EffectRefH effH = nullptr;
                                    if (efs->AEGP_GetLayerEffectByIndex(aegpId, layerH, fi, &effH) == A_Err_NONE && effH) {
                                        AEGP_InstalledEffectKey key = 0;
                                        A_char em[64] = "";
                                        efs->AEGP_GetInstalledKeyFromLayerEffect(effH, &key);
                                        efs->AEGP_GetEffectMatchName(key, em);
                                        /* must be the SAME variant the parade stream showed */
                                        if (strcmp(em, match) != 0) { MincLog("sync: index misalign '%s' vs '%s'", em, match); efs->AEGP_DisposeEffect(effH); effH = nullptr; }
                                    }
                                    if (effH) {
                                        /* transport: CallGeneric -> seq data + registry (names are the durable store; auto-sync re-derives on project open) */
                                        MincSyncPayload pay; memset(&pay, 0, sizeof(pay));
                                        pay.magic = MINC_ARB_MAGIC; pay.arb = want;
                                        pay.payVersion = 3;
                                        if (passBase[0]) {           /* refresh when healthy; empty = receiver keeps its passport */
                                            strncpy(pay.configBase, passBase, sizeof(pay.configBase) - 1);
                                            strncpy(pay.passportWorking, snap.workingSpace, sizeof(pay.passportWorking) - 1);
                                        }
                                        A_Time tg = {0, 100};
                                        if (efs->AEGP_EffectCallGeneric(aegpId, effH, &tg, PF_Cmd_COMPLETELY_GENERAL, &pay) == A_Err_NONE) {
                                            /* registry is keyed by instanceId: an id already claimed in THIS walk belongs to
                                               another instance (pre-1.3.1 mints restarted at 1 every session) -> re-mint */
                                            if (pay.outId == 0 || idsThisWalk.count(pay.outId)) {
                                                uint32_t fresh = MincMintInstanceId();
                                                while (idsThisWalk.count(fresh)) fresh = MincMintInstanceId();
                                                uint32_t old = pay.outId;
                                                pay.newId = fresh; pay.outId = 0;
                                                if (efs->AEGP_EffectCallGeneric(aegpId, effH, &tg, PF_Cmd_COMPLETELY_GENERAL, &pay) == A_Err_NONE && pay.outId == fresh) {
                                                    ++wrote; ++reminted; MincLog("sync: re-minted duplicate instance id %u -> %u ('%s')", old, fresh, name8);
                                                } else if (old) {
                                                    ++wrote; MincLog("sync: re-mint FAILED for id %u ('%s')", old, name8);
                                                } else {
                                                    /* handler never answered (outId stayed 0 twice): the effect binary
                                                       isn't installed — a placeholder instance, not a real write */
                                                    ++placeholders; MincLog("sync: no responder for '%s' (placeholder effect?)", name8);
                                                }
                                            } else ++wrote;
                                            if (pay.outId) idsThisWalk.insert(pay.outId);
                                        }
                                        efs->AEGP_DisposeEffect(effH);
                                    }
                                } else if (name8[0]) { ++badname; MincLog("sync: unparsed name '%s'", name8); }
                            }
                            sts->AEGP_DisposeStream(fxRef);
                        }
                        sts->AEGP_DisposeStream(fxGroup);
                    }
                    sts->AEGP_DisposeStream(layerRef);
                }
            }
        }
        AEGP_ItemH nextH = nullptr;
        its->AEGP_GetNextProjItem(projH, itemH, &nextH);
        itemH = nextH;
    }
    MincLog("sync: instances=%d wrote=%d unparsed=%d reminted=%d placeholders=%d christened=%d",
            seen, wrote, badname, reminted, placeholders, christened);
    return reminted;
}

bool MincParseGrammarVerb(MincVerb verb, const char *utf8, MinColorArb *out) {
    return ParseGrammar(verb, utf8, out);            /* badge edit builds its payload with the
                                                        same grammar the walk enforces */
}

void MincSyncFromNames(SPBasicSuite *bp, AEGP_PluginID aegpId, bool christen) {
    /* two bounded passes: within one walk a later duplicate's first CallGeneric clobbers the
       first claimant's registry entry before re-minting (serialization audit); whenever any
       id was re-minted, one repeat re-writes every instance under now-unique ids. Converges —
       pass 2 mints nothing (and never christens).                                          */
    if (SyncOnce(bp, aegpId, christen) > 0) SyncOnce(bp, aegpId, false);
}
