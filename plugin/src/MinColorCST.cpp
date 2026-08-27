#include "MinColorCST.h"

static PF_Err About(PF_InData *in_data, PF_OutData *out_data) {
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg,
        "%s v%d.%d — late-binding OCIO colorspace transform (minColor stage two).\n"
        "Destination is always the project's CURRENT working space.",
        MINC_NAME, MINC_MAJOR_VERSION, MINC_MINOR_VERSION);
    return PF_Err_NONE;
}

static PF_Err GlobalSetup(PF_InData *in_data, PF_OutData *out_data) {
    out_data->my_version = PF_VERSION(MINC_MAJOR_VERSION, MINC_MINOR_VERSION,
                                      MINC_BUG_VERSION, MINC_STAGE_VERSION, MINC_BUILD_VERSION);
    out_data->out_flags  = PF_OutFlag_DEEP_COLOR_AWARE |
                           PF_OutFlag_PIX_INDEPENDENT  |
                           PF_OutFlag_SEND_UPDATE_PARAMS_UI |
                           PF_OutFlag_SEQUENCE_DATA_NEEDS_FLATTENING;   /* render clones come from the flat snapshot — keep it fresh */
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_SMART_RENDER |
                           PF_OutFlag2_FLOAT_COLOR_AWARE     |
                           PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
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
                          0, 0,                       /* no custom UI yet (Ui.cpp = later): dims MUST be 0 without PF_OutFlag_CUSTOM_UI */
                          0,
                          PF_PUI_NO_ECW_UI,
                          arbH,
                          ARB_DISK_ID,
                          NULL);
    }
    if (!err) {
        AEFX_CLR_STRUCT(def);
        /* scriptable per-instance serial: the panel stamps a unique value on every instance so
           byte-identical comps can never share a cached frame, and bumps it after each sync to
           invalidate stale frames. Floats are the one param type ExtendScript can set. */
        PF_ADD_FLOAT_SLIDERX("Sync Serial", 0, 1000000, 0, 1000000, 0, PF_Precision_INTEGER, 0, 0, SERIAL_DISK_ID);
    }
    out_data->num_params = MINC_NUM_PARAMS;
    return err;
}

/* ---- sequence data: one flat MinColorArb per instance — the render-time source of truth.
        Written ONLY via PF_Cmd_COMPLETELY_GENERAL (our AEGP sync hands us the parsed name). ---- */
#include <atomic>
static std::atomic<uint32_t> g_instanceCounter{1};
static PF_Err SeqNew(PF_InData *in_data, PF_OutData *out_data, const MinColorArb *initial) {
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    PF_Handle h = suites.HandleSuite1()->host_new_handle(sizeof(MinColorArb));
    if (!h) return PF_Err_OUT_OF_MEMORY;
    MinColorArb *a = reinterpret_cast<MinColorArb*>(suites.HandleSuite1()->host_lock_handle(h));
    if (initial) *a = *initial;
    else { memset(a, 0, sizeof(*a)); a->magic = MINC_ARB_MAGIC; a->version = MINC_ARB_VERSION; }
    if (!a->instanceId) a->instanceId = g_instanceCounter.fetch_add(1);
    suites.HandleSuite1()->host_unlock_handle(h);
    out_data->sequence_data = h;
    return PF_Err_NONE;
}
static PF_Err SequenceSetup(PF_InData *in_data, PF_OutData *out_data) { return SeqNew(in_data, out_data, nullptr); }
static PF_Err SequenceResetup(PF_InData *in_data, PF_OutData *out_data) {
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    const MinColorArb *prev = nullptr;
    MinColorArb copy;
    if (in_data->sequence_data) {
        const MinColorArb *p = reinterpret_cast<const MinColorArb*>(
            suites.HandleSuite1()->host_lock_handle(in_data->sequence_data));
        if (p && p->magic == MINC_ARB_MAGIC && p->version == MINC_ARB_VERSION) { copy = *p; prev = &copy; }
        suites.HandleSuite1()->host_unlock_handle(in_data->sequence_data);
    }
    return SeqNew(in_data, out_data, prev);                 /* flat struct: resetup == copy */
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
    const MincSyncPayload *pay = reinterpret_cast<const MincSyncPayload*>(extra);
    if (!pay || pay->magic != MINC_ARB_MAGIC) return PF_Err_NONE;   /* not ours */
    if (params && params[MINC_ARB] && params[MINC_ARB]->u.arb_d.value) {
        /* the blessed transport: we are a PF context with live params — write our own arb
           param and flag CHANGED_VALUE so it round-trips through AE's normal machinery */
        AEGP_SuiteHandler suites(in_data->pica_basicP);
        MinColorArb *pa = reinterpret_cast<MinColorArb*>(
            suites.HandleSuite1()->host_lock_handle(params[MINC_ARB]->u.arb_d.value));
        if (pa) {
            uint32_t keepId = pa->instanceId;
            *pa = pay->arb; pa->instanceId = keepId;
            MincDebugLog("generic: param write space='%s' dir=%d id=%u", pa->space, (int)pa->direction, keepId);
        }
        suites.HandleSuite1()->host_unlock_handle(params[MINC_ARB]->u.arb_d.value);
        params[MINC_ARB]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
    }
    if (!in_data->sequence_data) { PF_Err e = SeqNew(in_data, out_data, &pay->arb); MincDebugLog("generic: seq created space='%s'", pay->arb.space); return e; }
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    MinColorArb *a = reinterpret_cast<MinColorArb*>(suites.HandleSuite1()->host_lock_handle(in_data->sequence_data));
    if (a) {
        uint32_t keep = a->instanceId;
        *a = pay->arb; a->instanceId = keep;
        MincRegistrySet(keep, a);                                    /* the render-visible copy */
        MincDebugLog("generic: id=%u space='%s' dir=%d -> registry", keep, a->space, (int)a->direction);
    }
    suites.HandleSuite1()->host_unlock_handle(in_data->sequence_data);
    out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
    return PF_Err_NONE;
}

extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                                       PF_ParamDef *params[], PF_LayerDef *output, void *extra) {
    PF_Err err = PF_Err_NONE;
    try {
        switch (cmd) {
            case PF_Cmd_ABOUT:               err = About(in_data, out_data);        break;
            case PF_Cmd_GLOBAL_SETUP:        err = GlobalSetup(in_data, out_data);  break;
            case PF_Cmd_PARAMS_SETUP:        err = ParamsSetup(in_data, out_data);  break;
            case PF_Cmd_ARBITRARY_CALLBACK:
                err = MincHandleArbitrary(in_data, out_data, params, output,
                                          reinterpret_cast<PF_ArbParamsExtra*>(extra));
                break;
            case PF_Cmd_SEQUENCE_SETUP:      err = SequenceSetup(in_data, out_data);   MincAuthorityRefresh(in_data); break;
            case PF_Cmd_SEQUENCE_RESETUP:    err = SequenceResetup(in_data, out_data); MincAuthorityRefresh(in_data); break;
            case PF_Cmd_SEQUENCE_FLATTEN:    err = SequenceResetup(in_data, out_data); break;   /* data is already flat: "flatten" = fresh copy */
            case PF_Cmd_GET_FLATTENED_SEQUENCE_DATA: err = SequenceResetup(in_data, out_data); break;
            case PF_Cmd_SEQUENCE_SETDOWN:    err = SequenceSetdown(in_data, out_data); break;
            case PF_Cmd_COMPLETELY_GENERAL:  err = HandleGeneric(in_data, out_data, params, extra); break;
            case PF_Cmd_UPDATE_PARAMS_UI:
            case PF_Cmd_USER_CHANGED_PARAM:  MincAuthorityRefresh(in_data);         break;
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
