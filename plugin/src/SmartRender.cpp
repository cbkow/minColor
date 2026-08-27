/* SmartFX CPU path: ARGB world -> RGBA float row scratch -> OCIO -> back. Alpha untouched. */
#include "MinColorCST.h"
#include <vector>
#include <cstring>

static PF_Err CheckoutArb(PF_InData *in_data, MinColorArb *out) {
    PF_ParamDef def;
    AEFX_CLR_STRUCT(def);
    PF_Err err = PF_CHECKOUT_PARAM(in_data, MINC_ARB, in_data->current_time,
                                   in_data->time_step, in_data->time_scale, &def);
    if (!err) {
        AEGP_SuiteHandler suites(in_data->pica_basicP);
        if (def.u.arb_d.value) {
            const MinColorArb *a = reinterpret_cast<const MinColorArb*>(
                suites.HandleSuite1()->host_lock_handle(def.u.arb_d.value));
            *out = *a;
            suites.HandleSuite1()->host_unlock_handle(def.u.arb_d.value);
        } else { memset(out, 0, sizeof(*out)); }
        PF_CHECKIN_PARAM(in_data, &def);
    }
    return err;
}

void MincResolveSeq(PF_InData *in_data, MincSeqData *sd) {
    /* ALWAYS read the seq clone: it carries instanceId + the passport, which the param never has.
       Precedence: seq clone -> registry (fresher, main-thread-written) -> param arb fields. */
    memset(sd, 0, sizeof(*sd));
    PF_ConstHandle ch = nullptr;
    const void *ssV = nullptr;
    if (in_data->pica_basicP->AcquireSuite(kPFEffectSequenceDataSuite, kPFEffectSequenceDataSuiteVersion1, &ssV) == kSPNoError && ssV) {
        const PF_EffectSequenceDataSuite1 *ss = reinterpret_cast<const PF_EffectSequenceDataSuite1*>(ssV);
        ss->PF_GetConstSequenceData(in_data->effect_ref, &ch);
        in_data->pica_basicP->ReleaseSuite(kPFEffectSequenceDataSuite, kPFEffectSequenceDataSuiteVersion1);
    }
    const MincSeqData *sa = ch ? *reinterpret_cast<const MincSeqData* const*>(ch) : nullptr;
    if (!sa && in_data->sequence_data) sa = *reinterpret_cast<const MincSeqData* const*>(in_data->sequence_data);
    if (sa && sa->arb.magic == MINC_ARB_MAGIC && sa->seqVersion == MINC_SEQ_VERSION) {
        *sd = *sa;
        MincSeqData reg;
        if (MincRegistryGet(sa->arb.instanceId, &reg)) { reg.arb.instanceId = sa->arb.instanceId; *sd = reg; }
    }
    MinColorArb pa; memset(&pa, 0, sizeof(pa));
    CheckoutArb(in_data, &pa);
    if (pa.space[0]) {                                    /* param overrides ARB FIELDS only — its own
                                                             nonce id and absent passport must not win */
        sd->arb.direction = pa.direction;
        memcpy(sd->arb.space, pa.space, sizeof(sd->arb.space));
        sd->arb.magic = MINC_ARB_MAGIC; sd->arb.version = MINC_ARB_VERSION;
    }
    MincDebugLog("resolve: seqId=%u space='%s' dir=%d base='%s'", sd->arb.instanceId, sd->arb.space, (int)sd->arb.direction, sd->configBase);
}

void MincResolveArb(PF_InData *in_data, MinColorArb *arb) {      /* thin wrapper: arb view of the seq resolve */
    MincSeqData sd; MincResolveSeq(in_data, &sd);
    *arb = sd.arb;
}

PF_Err MincSmartPreRender(PF_InData *in_data, PF_OutData *out_data, PF_PreRenderExtra *extra) {
    PF_Err err = PF_Err_NONE;
    {   /* our real state is invisible to AE's param fingerprint — mix the EFFECTIVE state
           (resolved arb + the authority the render will actually use) into the render GUID */
        MincSeqData sd; MincResolveSeq(in_data, &sd);
        MincAuthoritySnapshot live = {}; MincAuthorityGet(&live);
        MincAuthoritySnapshot eff = {};
        uint32_t usedPassport = MincEffectiveAuthority(&live, &sd, &eff) ? 1u : 0u;
        struct { MinColorArb a; uint32_t pass; char cfg[1024]; char ws[MINC_SPACE_LEN]; } mix;
        memset(&mix, 0, sizeof(mix));                    /* padding must not leak stack noise into the GUID */
        mix.a = sd.arb; mix.pass = usedPassport;
        memcpy(mix.cfg, eff.configPath, sizeof(mix.cfg));
        memcpy(mix.ws, eff.workingSpace, sizeof(mix.ws));
        extra->cb->GuidMixInPtr(in_data->effect_ref, sizeof(mix), &mix);
    }
    PF_RenderRequest req = extra->input->output_request;
    PF_CheckoutResult res;
    ERR(extra->cb->checkout_layer(in_data->effect_ref, MINC_INPUT, MINC_INPUT, &req,
                                  in_data->current_time, in_data->time_step, in_data->time_scale, &res));
    if (!err) {
        extra->output->result_rect     = res.result_rect;
        extra->output->max_result_rect = res.max_result_rect;
    }
    return err;
}

template <typename PIX, typename CVT_IN, typename CVT_OUT>
static void ProcessRows(const PF_EffectWorld *in, PF_EffectWorld *out,
                        const MincAuthoritySnapshot *auth, const MinColorArb *arb,
                        int *statusOut, CVT_IN cvtIn, CVT_OUT cvtOut) {
    const int w = MIN(in->width, out->width), h = MIN(in->height, out->height);
    std::vector<float> scratch((size_t)w * 4);
    for (int y = 0; y < h; ++y) {
        const PIX *ip = reinterpret_cast<const PIX*>((const char*)in->data  + (size_t)y * in->rowbytes);
        PIX       *op = reinterpret_cast<PIX*>(      (char*)out->data       + (size_t)y * out->rowbytes);
        for (int x = 0; x < w; ++x) {                       /* AE pixels are ARGB */
            scratch[(size_t)x*4+0] = cvtIn(ip[x].red);
            scratch[(size_t)x*4+1] = cvtIn(ip[x].green);
            scratch[(size_t)x*4+2] = cvtIn(ip[x].blue);
            scratch[(size_t)x*4+3] = 1.0f;                  /* alpha not transformed */
        }
        int st = MincOcioApplyRows(auth, arb, scratch.data(), w);
        if (y == 0) *statusOut = st;
        for (int x = 0; x < w; ++x) {
            op[x].alpha = ip[x].alpha;
            if (st == MINC_STATUS_OK) {
                op[x].red   = cvtOut(scratch[(size_t)x*4+0]);
                op[x].green = cvtOut(scratch[(size_t)x*4+1]);
                op[x].blue  = cvtOut(scratch[(size_t)x*4+2]);
            } else { op[x].red = ip[x].red; op[x].green = ip[x].green; op[x].blue = ip[x].blue; }
        }
    }
}

PF_Err MincSmartRender(PF_InData *in_data, PF_OutData *out_data, PF_SmartRenderExtra *extra) {
    PF_Err err = PF_Err_NONE;
    PF_EffectWorld *inputW = nullptr, *outputW = nullptr;
    ERR(extra->cb->checkout_layer_pixels(in_data->effect_ref, MINC_INPUT, &inputW));
    ERR(extra->cb->checkout_output(in_data->effect_ref, &outputW));
    if (!err && inputW && outputW) {
        MincSeqData sd; MincResolveSeq(in_data, &sd);
        MinColorArb arb = sd.arb;
        PF_Err arbErr = PF_Err_NONE;
        MincAuthoritySnapshot live = {};
        MincAuthorityGet(&live);
        MincAuthoritySnapshot auth = {};
        MincEffectiveAuthority(&live, &sd, &auth);       /* dead live authority + passport -> local store */
        MincDebugLog("render: arbErr=%d magic=%08lx dir=%d space='%s' | gen=%lu ocioOn=%d ws='%s'",
                     (int)arbErr, (unsigned long)arb.magic, (int)arb.direction, arb.space,
                     auth.generation, (int)auth.ocioOn, auth.workingSpace);
        int status = MINC_STATUS_PASS_EMPTY;
        PF_PixelFormat fmt = PF_PixelFormat_INVALID;
        {
            const void *wsV = nullptr;
            if (in_data->pica_basicP->AcquireSuite(kPFWorldSuite, kPFWorldSuiteVersion2, &wsV) == kSPNoError && wsV) {
                const PF_WorldSuite2 *ws = reinterpret_cast<const PF_WorldSuite2*>(wsV);
                ws->PF_GetPixelFormat(outputW, &fmt);
                in_data->pica_basicP->ReleaseSuite(kPFWorldSuite, kPFWorldSuiteVersion2);
            }
        }
        switch (fmt) {
            case PF_PixelFormat_ARGB128:
                ProcessRows<PF_PixelFloat>(inputW, outputW, &auth, &arb, &status,
                    [](float v){ return v; },
                    [](float v){ return v; });
                break;
            case PF_PixelFormat_ARGB64:
                ProcessRows<PF_Pixel16>(inputW, outputW, &auth, &arb, &status,
                    [](A_u_short v){ return (float)v / 32768.0f; },
                    [](float v){ float c = v < 0.f ? 0.f : (v > 1.f ? 1.f : v); return (A_u_short)(c * 32768.0f + 0.5f); });
                break;
            case PF_PixelFormat_ARGB32:
            default:
                ProcessRows<PF_Pixel8>(inputW, outputW, &auth, &arb, &status,
                    [](A_u_char v){ return (float)v / 255.0f; },
                    [](float v){ float c = v < 0.f ? 0.f : (v > 1.f ? 1.f : v); return (A_u_char)(c * 255.0f + 0.5f); });
                break;
        }
        MincDebugLog("render: status=%d fmt=%d", status, (int)fmt);
    }
    if (inputW) extra->cb->checkin_layer_pixels(in_data->effect_ref, MINC_INPUT);
    return err;
}
