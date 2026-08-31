#include "MinColorCST.h"

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
    out_data->out_flags  = PF_OutFlag_CUSTOM_UI |
                           PF_OutFlag_DEEP_COLOR_AWARE |
                           PF_OutFlag_PIX_INDEPENDENT  |
                           PF_OutFlag_SEND_UPDATE_PARAMS_UI |
                           PF_OutFlag_SEQUENCE_DATA_NEEDS_FLATTENING;   /* render clones come from the flat snapshot — keep it fresh */
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_SMART_RENDER |
                           PF_OutFlag2_FLOAT_COLOR_AWARE     |
                           PF_OutFlag2_SUPPORTS_THREADED_RENDERING |
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
                          210, 30,                    /* the ECW badge (Ui.cpp) */
                          0,
                          PF_PUI_CONTROL,
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
    if (!err) {                                                     /* register for ECW events */
        PF_CustomUIInfo ci;
        AEFX_CLR_STRUCT(ci);
        ci.events = PF_CustomEFlag_EFFECT;
        ci.comp_ui_width = ci.comp_ui_height = 0;  ci.comp_ui_alignment = PF_UIAlignment_NONE;
        ci.layer_ui_width = ci.layer_ui_height = 0; ci.layer_ui_alignment = PF_UIAlignment_NONE;
        ci.preview_ui_width = ci.preview_ui_height = 0; ci.preview_ui_alignment = PF_UIAlignment_NONE;
        err = (*(in_data->inter.register_ui))(in_data->effect_ref, &ci);
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
            case PF_Cmd_SEQUENCE_SETUP:
                /* FRESH instance (a drop, a paste): arm the AEGP's christening walk via the
                   shared marker. RESETUP never arms — undo of a christening arrives as
                   RESETUP, and the reverted default name must STAY reverted.              */
                err = SequenceSetup(in_data, out_data);
                MincTouchWalkMarker("christen");
                MincAuthorityRefresh(in_data);
                break;
            case PF_Cmd_SEQUENCE_RESETUP:    err = SequenceResetup(in_data, out_data); MincAuthorityRefresh(in_data); break;
            case PF_Cmd_SEQUENCE_FLATTEN:    MincDebugLog("seq: cmd FLATTEN"); err = SequenceResetup(in_data, out_data); break;
            case PF_Cmd_GET_FLATTENED_SEQUENCE_DATA: MincDebugLog("seq: cmd GET_FLATTENED"); err = SequenceResetup(in_data, out_data); break;
            case PF_Cmd_SEQUENCE_SETDOWN:    err = SequenceSetdown(in_data, out_data); break;
            case PF_Cmd_COMPLETELY_GENERAL:  err = HandleGeneric(in_data, out_data, params, extra); break;
            case PF_Cmd_UPDATE_PARAMS_UI:
            case PF_Cmd_USER_CHANGED_PARAM:  MincAuthorityRefresh(in_data);         break;
            case PF_Cmd_EVENT:
                err = MincHandleEvent(verb, in_data, out_data, params, reinterpret_cast<PF_EventExtra*>(extra));
                break;
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

extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                                       PF_ParamDef *params[], PF_LayerDef *output, void *extra) {
    return EffectMainCommon(MINC_VERB_LEGACY, cmd, in_data, out_data, params, output, extra);
}
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
