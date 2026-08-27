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
                           PF_OutFlag_SEND_UPDATE_PARAMS_UI;
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
        def.flags = PF_ParamFlag_CANNOT_TIME_VARY;   /* keyframing this makes no sense in v1 */
        PF_ADD_ARBITRARY2("Transform",
                          1, 1,                       /* no custom UI yet (Ui.cpp = later)   */
                          0,
                          PF_PUI_NO_ECW_UI,
                          arbH,
                          ARB_DISK_ID,
                          NULL);
    }
    out_data->num_params = MINC_NUM_PARAMS;
    return err;
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
            case PF_Cmd_SEQUENCE_RESETUP:    /* fallthrough: both are main-thread moments   */
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
