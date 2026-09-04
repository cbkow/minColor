#include "MinColorCST.h"
#include "MincMenus.h"          /* per-verb popup choices (AEGP-written plugin-menus.json) */
#include "AEFX_SuiteHelper.h"   /* AEFX_AcquireSuite for PF_TouchActiveItem */

static const char *VerbEffectName(MincVerb verb) {
    switch (verb) {
        case MINC_VERB_XFORM:  return MINC_NAME_XFORM;
        case MINC_VERB_VIEW:   return MINC_NAME_VIEW;
        case MINC_VERB_RENDER: return MINC_NAME_RENDER;
        case MINC_VERB_LOOK:   return MINC_NAME_LOOK;
        default:               return MINC_NAME_LEGACY;
    }
}

static PF_Err About(MincVerb verb, PF_InData *in_data, PF_OutData *out_data) {
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg,
        "%s v%d.%d — late-binding OCIO colorspace transform.\n"
        "The match name carries the verb; the effect name carries the space.\n"
        "Destination is always the project's CURRENT working space.",
        VerbEffectName(verb), MINC_MAJOR_VERSION, MINC_MINOR_VERSION);
    return PF_Err_NONE;
}

static PF_Err GlobalSetup(PF_InData *in_data, PF_OutData *out_data) {
    out_data->my_version = PF_VERSION(MINC_MAJOR_VERSION, MINC_MINOR_VERSION,
                                      MINC_BUG_VERSION, MINC_STAGE_VERSION, MINC_BUILD_VERSION);
    out_data->out_flags  = PF_OutFlag_DEEP_COLOR_AWARE |             /* lean-v3: CUSTOM_UI dropped — the
                                                                        native popup replaces the badge */
                           PF_OutFlag_PIX_INDEPENDENT  |
                           PF_OutFlag_SEND_UPDATE_PARAMS_UI |
                           PF_OutFlag_SEQUENCE_DATA_NEEDS_FLATTENING;   /* render clones come from the flat snapshot — keep it fresh */
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_SMART_RENDER |
                           PF_OutFlag2_FLOAT_COLOR_AWARE     |
                           PF_OutFlag2_SUPPORTS_THREADED_RENDERING |
                           PF_OutFlag2_SUPPORTS_GET_FLATTENED_SEQUENCE_DATA |   /* required with NEEDS_FLATTENING
                                                                                  + THREADED_RENDERING, AND for
                                                                                  FORCE_RERENDER in mouse events */
                           PF_OutFlag2_I_MIX_GUID_DEPENDENCIES;   /* we call GuidMixInPtr in PreRender */
    MincAuthorityGlobalSetup(in_data);          /* register with AEGP, install hooks */
    return PF_Err_NONE;
}

static PF_Err ParamsSetup(PF_InData *in_data, PF_OutData *out_data) {
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def;
    AEFX_CLR_STRUCT(def);
    PF_ArbitraryH arbH = NULL;
    ERR(MincArbNewDefault(in_data, &arbH));
    if (!err) {
        def.flags = 0;   /* CANNOT_TIME_VARY made AE serve the param from static storage, ignoring AEGP stream writes (M3 finding) */
        PF_ADD_ARBITRARY2("Transform",
                          0, 0,                       /* lean-v3: hidden truth store, no UI (was the ECW badge) */
                          0,
                          PF_PUI_INVISIBLE,
                          arbH,
                          ARB_DISK_ID,
                          NULL);
    }
    if (!err) {
        AEFX_CLR_STRUCT(def);
        /* scriptable per-instance serial: the panel stamps a unique value on every instance so
           byte-identical comps can never share a cached frame, and bumps it after each sync to
           invalidate stale frames. Floats are the one param type ExtendScript can set. */
        /* NOT the X macro: its 8th arg is DISP (display flags), not ui_flags — passing
           PF_PUI_INVISIBLE there left the param visible. Set ui_flags explicitly. */
        AEFX_CLR_STRUCT(def);
        def.ui_flags = PF_PUI_INVISIBLE;
        PF_ADD_FLOAT_SLIDER("Sync Serial", 0, 1000000, 0, 1000000, 0, 0, PF_Precision_INTEGER, 0, 0, SERIAL_DISK_ID);
    }
    if (!err) {
        /* native colorspace popup — the list is rebuilt per-verb from the config in
           UPDATE_PARAMS_UI; SUPERVISE fires USER_CHANGED_PARAM so a pick writes the arb.
           A pure UI mirror: the NAME in the arb stays the render-truth (Phase 0 de-risk). */
        AEFX_CLR_STRUCT(def);
        def.flags = PF_ParamFlag_SUPERVISE;
        PF_ADD_POPUP("Space", 1, 1, "(config)", POPUP_DISK_ID);
    }
    out_data->num_params = MINC_NUM_PARAMS;
    return err;
}

/* ---- sequence data: one flat MinColorArb per instance — the render-time source of truth.
        Written ONLY via PF_Cmd_COMPLETELY_GENERAL (the AEGP sync hands us the parsed name).
        MincMintInstanceId lives in core (the walk mints from either binary).             ---- */
static PF_Err SeqNew(PF_InData *in_data, PF_OutData *out_data, const MincSeqData *initial) {
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    PF_Handle h = suites.HandleSuite1()->host_new_handle(sizeof(MincSeqData));
    if (!h) return PF_Err_OUT_OF_MEMORY;
    MincDebugLog("seq: NEW handle size=%lu initial=%d", (unsigned long)sizeof(MincSeqData), initial ? 1 : 0);
    MincSeqData *sd = reinterpret_cast<MincSeqData*>(suites.HandleSuite1()->host_lock_handle(h));
    if (initial) *sd = *initial;
    else { memset(sd, 0, sizeof(*sd)); sd->arb.magic = MINC_ARB_MAGIC; sd->arb.version = MINC_ARB_VERSION; }
    sd->seqVersion = MINC_SEQ_VERSION;
    if (!sd->arb.instanceId) sd->arb.instanceId = MincMintInstanceId();
    suites.HandleSuite1()->host_unlock_handle(h);
    out_data->sequence_data = h;
    return PF_Err_NONE;
}
static PF_Err SequenceSetup(PF_InData *in_data, PF_OutData *out_data) { return SeqNew(in_data, out_data, nullptr); }
static PF_Err SequenceResetup(PF_InData *in_data, PF_OutData *out_data) {
    /* dual-accept: a v2 handle (this build), or the 212-byte v1 arb an old save carries —
       upgraded in place with an empty passport (filled at the next healthy sync). */
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    const MincSeqData *prev = nullptr;
    MincSeqData copy;
    if (in_data->sequence_data) {
        AEGP_MemSize sz = suites.HandleSuite1()->host_get_handle_size(in_data->sequence_data);
        MincDebugLog("seq: RESETUP in-size=%lu", (unsigned long)sz);
        const void *raw = suites.HandleSuite1()->host_lock_handle(in_data->sequence_data);
        const MinColorArb *p = reinterpret_cast<const MinColorArb*>(raw);
        if (p && p->magic == MINC_ARB_MAGIC && p->version == MINC_ARB_VERSION) {
            if ((size_t)sz >= sizeof(MincSeqData) &&
                reinterpret_cast<const MincSeqData*>(raw)->seqVersion == MINC_SEQ_VERSION) {
                copy = *reinterpret_cast<const MincSeqData*>(raw); prev = &copy;
            } else if ((size_t)sz >= sizeof(MinColorArb)) {
                memset(&copy, 0, sizeof(copy)); copy.arb = *p; prev = &copy;
            }
        }
        suites.HandleSuite1()->host_unlock_handle(in_data->sequence_data);
    }
    return SeqNew(in_data, out_data, prev);
}
static PF_Err SequenceSetdown(PF_InData *in_data, PF_OutData *out_data) {
    if (in_data->sequence_data) {
        AEGP_SuiteHandler suites(in_data->pica_basicP);
        suites.HandleSuite1()->host_dispose_handle(in_data->sequence_data);
    }
    out_data->sequence_data = NULL;
    return PF_Err_NONE;
}
static PF_Err HandleGeneric(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], void *extra) {
    MincSyncPayload *pay = reinterpret_cast<MincSyncPayload*>(extra);
    if (!pay || pay->magic != MINC_ARB_MAGIC) return PF_Err_NONE;   /* not ours */
    const uint32_t adopt = (pay->payVersion >= 3) ? pay->newId : 0; /* duplicate repair: sender asks us to take a fresh id */
    if (pay->payVersion >= 3) pay->outId = 0;
    if (params && params[MINC_ARB] && params[MINC_ARB]->u.arb_d.value) {
        /* the blessed transport: we are a PF context with live params — write our own arb
           param and flag CHANGED_VALUE so it round-trips through AE's normal machinery */
        AEGP_SuiteHandler suites(in_data->pica_basicP);
        MinColorArb *pa = reinterpret_cast<MinColorArb*>(
            suites.HandleSuite1()->host_lock_handle(params[MINC_ARB]->u.arb_d.value));
        if (pa) {
            uint32_t keepId = adopt ? adopt : pa->instanceId;
            *pa = pay->arb; pa->instanceId = keepId;
            MincDebugLog("generic: param write space='%s' dir=%d id=%u", pa->space, (int)pa->direction, keepId);
        }
        suites.HandleSuite1()->host_unlock_handle(params[MINC_ARB]->u.arb_d.value);
        params[MINC_ARB]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
    }
    if (!in_data->sequence_data) {
        MincSeqData init; memset(&init, 0, sizeof(init)); init.arb = pay->arb; init.arb.instanceId = adopt;   /* 0 -> SeqNew mints */
        if (pay->payVersion >= 2 && pay->configBase[0]) {
            memcpy(init.configBase, pay->configBase, sizeof(init.configBase));
            memcpy(init.passportWorking, pay->passportWorking, sizeof(init.passportWorking));
        }
        PF_Err e = SeqNew(in_data, out_data, &init); MincDebugLog("generic: seq created space='%s'", pay->arb.space);
        if (pay->payVersion >= 3 && out_data->sequence_data) {
            AEGP_SuiteHandler s2(in_data->pica_basicP);
            const MincSeqData *ns = reinterpret_cast<const MincSeqData*>(s2.HandleSuite1()->host_lock_handle(out_data->sequence_data));
            if (ns) pay->outId = ns->arb.instanceId;
            s2.HandleSuite1()->host_unlock_handle(out_data->sequence_data);
        }
        return e;
    }
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    MincSeqData *sd = reinterpret_cast<MincSeqData*>(suites.HandleSuite1()->host_lock_handle(in_data->sequence_data));
    if (sd) {
        uint32_t keep = adopt ? adopt : sd->arb.instanceId;
        if (!keep) keep = MincMintInstanceId();
        sd->arb = pay->arb; sd->arb.instanceId = keep;
        if (pay->payVersion >= 3) pay->outId = keep;
        if (pay->payVersion >= 2 && pay->configBase[0]) {            /* refresh when healthy — NEVER cleared
                                                                        when the payload arrives passport-less */
            memcpy(sd->configBase, pay->configBase, sizeof(sd->configBase));
            memcpy(sd->passportWorking, pay->passportWorking, sizeof(sd->passportWorking));
        }
        MincRegistrySet(keep, sd);                                   /* the render-visible copy */
        MincDebugLog("generic: id=%u space='%s' dir=%d -> registry", keep, sd->arb.space, (int)sd->arb.direction);
    }
    suites.HandleSuite1()->host_unlock_handle(in_data->sequence_data);
    out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
    return PF_Err_NONE;
}

/* ---- native colorspace popup (Phase 0 de-risk: UI mirror, name stays the truth) ---- */
static uint16_t DirForVerb(MincVerb v) {
    if (v == MINC_VERB_LOOK) return MINC_DIR_LOOK;
    if (v == MINC_VERB_VIEW || v == MINC_VERB_RENDER) return MINC_DIR_FROM_WORKING;
    return MINC_DIR_TO_WORKING;   /* XFORM/input, LEGACY */
}

/* Build the per-verb choice list (curated menus file, else live OCIO enumeration), append the
   current arb.space if it isn't in the list, and report the 1-based selection. Populate + change
   share this so index<->name never drift. Returns count; fills items[], *sdOut, *selOut.       */
static int BuildVerbList(MincVerb verb, PF_InData *in_data, char items[][MINC_SPACE_LEN],
                         MincSeqData *sdOut, int *selOut) {
    MincSeqData sd; MincResolveSeq(in_data, &sd);
    if (sdOut) *sdOut = sd;
    MincAuthoritySnapshot live = {}; MincAuthorityGet(&live);
    MincAuthoritySnapshot auth = {}; MincEffectiveAuthority(&live, &sd, &auth);
    int n = 0;
    MincMenus menus;
    if (MincMenusGet(&menus)) {
        const char (*src)[MINC_SPACE_LEN] = menus.inputSpaces; int cnt = menus.nInput;
        if (verb == MINC_VERB_VIEW)   { src = menus.viewSpaces;   cnt = menus.nView; }
        if (verb == MINC_VERB_RENDER) { src = menus.renderSpaces; cnt = menus.nRender; }
        if (verb == MINC_VERB_LOOK)   { src = menus.looks;        cnt = menus.nLooks; }
        for (int i = 0; i < cnt && n < MINC_MENU_MAX; ++i) { snprintf(items[n], MINC_SPACE_LEN, "%s", src[i]); ++n; }
    }
    if (n == 0) n = (verb == MINC_VERB_LOOK) ? MincOcioListLooks(&auth, items, MINC_MENU_MAX)
                                             : MincOcioListSpaces(&auth, items, MINC_MENU_MAX);
    int sel = 0;
    for (int i = 0; i < n; ++i) if (sd.arb.space[0] && !strcmp(items[i], sd.arb.space)) { sel = i + 1; break; }
    if (sel == 0 && sd.arb.space[0] && n < MINC_MENU_MAX) {   /* current space not offered — show it, selected */
        snprintf(items[n], MINC_SPACE_LEN, "%s", sd.arb.space); ++n; sel = n;
    }
    if (sel == 0) sel = 1;
    if (selOut) *selOut = sel;
    return n;
}

static void PopulatePopup(MincVerb verb, PF_InData *in_data, PF_ParamDef *params[]) {
    if (!params || !params[MINC_POPUP]) return;
    static char items[MINC_MENU_MAX][MINC_SPACE_LEN];              /* UI is main-thread only */
    static char joined[MINC_MENU_MAX * MINC_SPACE_LEN];           /* namesptr must persist after the call */
    MincSeqData sd; int sel = 1;
    int n = BuildVerbList(verb, in_data, items, &sd, &sel);
    size_t off = 0; joined[0] = 0;
    if (n == 0) { snprintf(joined, sizeof(joined), "(no config)"); n = 1; sel = 1; }
    else for (int i = 0; i < n; ++i)
        off += snprintf(joined + off, sizeof(joined) - off, "%s%s", i ? "|" : "", items[i]);
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    PF_ParamDef pd = *params[MINC_POPUP];
    pd.param_type = PF_Param_POPUP;
    pd.u.pd.num_choices = (short)n;
    pd.u.pd.u.namesptr  = joined;
    pd.u.pd.value       = sel;
    suites.ParamUtilsSuite3()->PF_UpdateParamUI(in_data->effect_ref, MINC_POPUP, &pd);
    MincDebugLog("popup: populate verb=%d n=%d sel=%d space='%s'", (int)verb, n, sel, sd.arb.space);
}

static PF_Err HandleGeneric(PF_InData *, PF_OutData *, PF_ParamDef *[], void *);   /* fwd */

static void OnPopupChanged(MincVerb verb, PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[]) {
    if (!params || !params[MINC_POPUP]) return;
    static char items[MINC_MENU_MAX][MINC_SPACE_LEN];
    MincSeqData sd; int sel = 1;
    int n = BuildVerbList(verb, in_data, items, &sd, &sel);
    int val = params[MINC_POPUP]->u.pd.value;                     /* 1-based */
    if (val < 1 || val > n) { MincDebugLog("popup: change out-of-range val=%d n=%d", val, n); return; }
    if (!strcmp(items[val - 1], sd.arb.space)) return;           /* no change */
    /* write through the blessed transport (param + seq + registry + FORCE_RERENDER) */
    MincSyncPayload pay; memset(&pay, 0, sizeof(pay));
    pay.magic = MINC_ARB_MAGIC;
    pay.arb.magic = MINC_ARB_MAGIC; pay.arb.version = MINC_ARB_VERSION;
    pay.arb.direction = DirForVerb(verb);
    pay.arb.instanceId = sd.arb.instanceId;
    snprintf(pay.arb.space, MINC_SPACE_LEN, "%s", items[val - 1]);
    pay.payVersion = 2;
    MincAuthoritySnapshot live = {}; MincAuthorityGet(&live);
    if (live.ocioOn && live.configPath[0]) {                      /* stamp the passport from live authority */
        const char *b = strrchr(live.configPath, '/'); b = b ? b + 1 : live.configPath;
        snprintf(pay.configBase, MINC_CONFIGBASE_LEN, "%s", b);
        snprintf(pay.passportWorking, MINC_SPACE_LEN, "%s", live.workingSpace);
    }
    HandleGeneric(in_data, out_data, params, &pay);
    out_data->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
    PF_AdvItemSuite1 *adv = NULL;
    if (!AEFX_AcquireSuite(in_data, out_data, kPFAdvItemSuite, kPFAdvItemSuiteVersion1, NULL, (void **)&adv) && adv) {
        adv->PF_TouchActiveItem();
        AEFX_ReleaseSuite(in_data, out_data, kPFAdvItemSuite, kPFAdvItemSuiteVersion1, NULL);
    }
    MincDebugLog("popup: changed -> space='%s' dir=%d id=%u", pay.arb.space, (int)pay.arb.direction, pay.arb.instanceId);
}

/* one implementation, five registered effects: the wrapper passes the verb statically.
   The verb is About-only until M2 step 2 (verb authority lands in the walk).            */
static PF_Err EffectMainCommon(MincVerb verb, PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                               PF_ParamDef *params[], PF_LayerDef *output, void *extra) {
    PF_Err err = PF_Err_NONE;
    try {
        switch (cmd) {
            case PF_Cmd_ABOUT:               err = About(verb, in_data, out_data);  break;
            case PF_Cmd_GLOBAL_SETUP:        err = GlobalSetup(in_data, out_data);  break;
            case PF_Cmd_PARAMS_SETUP:        err = ParamsSetup(in_data, out_data);  break;
            case PF_Cmd_ARBITRARY_CALLBACK:
                err = MincHandleArbitrary(in_data, out_data, params, output,
                                          reinterpret_cast<PF_ArbParamsExtra*>(extra));
                break;
            case PF_Cmd_SEQUENCE_SETUP:                   /* lean-v3: no christening marker — daemon is gone */
                err = SequenceSetup(in_data, out_data);
                MincAuthorityRefresh(in_data);
                break;
            case PF_Cmd_SEQUENCE_RESETUP:    err = SequenceResetup(in_data, out_data); MincAuthorityRefresh(in_data); break;
            case PF_Cmd_SEQUENCE_FLATTEN:    MincDebugLog("seq: cmd FLATTEN"); err = SequenceResetup(in_data, out_data); break;
            case PF_Cmd_GET_FLATTENED_SEQUENCE_DATA: MincDebugLog("seq: cmd GET_FLATTENED"); err = SequenceResetup(in_data, out_data); break;
            case PF_Cmd_SEQUENCE_SETDOWN:    err = SequenceSetdown(in_data, out_data); break;
            case PF_Cmd_COMPLETELY_GENERAL:  err = HandleGeneric(in_data, out_data, params, extra); break;
            case PF_Cmd_UPDATE_PARAMS_UI:
                PopulatePopup(verb, in_data, params);
                MincAuthorityRefresh(in_data);
                break;
            case PF_Cmd_USER_CHANGED_PARAM: {
                PF_UserChangedParamExtra *uc = reinterpret_cast<PF_UserChangedParamExtra*>(extra);
                if (uc && uc->param_index == MINC_POPUP)
                    OnPopupChanged(verb, in_data, out_data, params);
                MincAuthorityRefresh(in_data);
                break;
            }
            case PF_Cmd_SMART_PRE_RENDER:
                err = MincSmartPreRender(in_data, out_data,
                                         reinterpret_cast<PF_PreRenderExtra*>(extra));
                break;
            case PF_Cmd_SMART_RENDER:
                err = MincSmartRender(in_data, out_data,
                                      reinterpret_cast<PF_SmartRenderExtra*>(extra));
                break;
            default: break;
        }
    } catch (...) { err = PF_Err_INTERNAL_STRUCT_DAMAGED; }   /* nothing throws across PF */
    return err;
}

#ifdef AE_OS_WIN
/* legacy "MINC CST" entry — Windows-only from M3 step 8 (mac dropped PiPL 16000;
   1.x instances there are benign placeholders that Migrate resurrects as variants) */
extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                                       PF_ParamDef *params[], PF_LayerDef *output, void *extra) {
    return EffectMainCommon(MINC_VERB_LEGACY, cmd, in_data, out_data, params, output, extra);
}
#endif
extern "C" DllExport PF_Err EffectMainXform(PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                                            PF_ParamDef *params[], PF_LayerDef *output, void *extra) {
    return EffectMainCommon(MINC_VERB_XFORM, cmd, in_data, out_data, params, output, extra);
}
extern "C" DllExport PF_Err EffectMainView(PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                                           PF_ParamDef *params[], PF_LayerDef *output, void *extra) {
    return EffectMainCommon(MINC_VERB_VIEW, cmd, in_data, out_data, params, output, extra);
}
extern "C" DllExport PF_Err EffectMainRender(PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                                             PF_ParamDef *params[], PF_LayerDef *output, void *extra) {
    return EffectMainCommon(MINC_VERB_RENDER, cmd, in_data, out_data, params, output, extra);
}
extern "C" DllExport PF_Err EffectMainLook(PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                                           PF_ParamDef *params[], PF_LayerDef *output, void *extra) {
    return EffectMainCommon(MINC_VERB_LOOK, cmd, in_data, out_data, params, output, extra);
}
