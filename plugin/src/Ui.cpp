/* ECW badge: direction + colorspace + live status chip, drawn with Drawbot. */
#include "MinColorCST.h"
#include "AEFX_SuiteHelper.h"
#include <cstring>
#include <cstdio>

static void Ascii16(const char *s, DRAWBOT_UTF16Char *out, int cap) {
    /* byte-per-slot mangled 2-byte UTF-8 (the middle dot drew as "A^·") — decode 2-byte sequences */
    int i = 0; const unsigned char *p = (const unsigned char *)s;
    while (*p && i < cap - 1) {
        if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            out[i++] = (DRAWBOT_UTF16Char)(((p[0] & 0x1F) << 6) | (p[1] & 0x3F)); p += 2;
        } else out[i++] = (DRAWBOT_UTF16Char)*p++;
    }
    out[i] = 0;
}

PF_Err MincHandleEvent(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_EventExtra *extra) {
    PF_Err err = PF_Err_NONE, err2 = PF_Err_NONE;
    if (extra->e_type != PF_Event_DRAW || extra->effect_win.area != PF_EA_CONTROL) return err;

    MincSeqData sdRes; MincResolveSeq(in_data, &sdRes);
    MinColorArb arb = sdRes.arb;
    MincAuthoritySnapshot live = {}; MincAuthorityGet(&live);
    MincAuthoritySnapshot auth = {};
    bool viaPassport = MincEffectiveAuthority(&live, &sdRes, &auth);
    int status = MincOcioProbeStatus(&auth, &arb);

    DRAWBOT_Suites suites; DRAWBOT_DrawRef drawRef = NULL;
    ERR(AEFX_AcquireDrawbotSuites(in_data, out_data, &suites));
    PF_EffectCustomUISuite1 *cui = NULL;
    ERR(AEFX_AcquireSuite(in_data, out_data, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, NULL, (void**)&cui));
    if (!err && cui) {
        ERR((*cui->PF_GetDrawingReference)(extra->contextH, &drawRef));
        AEFX_ReleaseSuite(in_data, out_data, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, NULL);
    }
    DRAWBOT_SupplierRef sup = NULL; DRAWBOT_SurfaceRef surf = NULL;
    ERR(suites.drawbot_suiteP->GetSupplier(drawRef, &sup));
    ERR(suites.drawbot_suiteP->GetSurface(drawRef, &surf));
    if (!err) {
        const PF_Rect fr = extra->effect_win.current_frame;
        DRAWBOT_RectF32 r = { fr.left + 0.5f, fr.top + 0.5f, (float)(fr.right - fr.left), (float)(fr.bottom - fr.top) };
        DRAWBOT_ColorRGBA bg;
        switch (status) {                                            /* chip = the ladder made visible */
            case MINC_STATUS_OK:               bg = viaPassport ? DRAWBOT_ColorRGBA{0.13f, 0.22f, 0.34f, 1}
                                                                : DRAWBOT_ColorRGBA{0.13f, 0.30f, 0.16f, 1}; break;
            case MINC_STATUS_PASS_EMPTY:       bg = {0.25f, 0.25f, 0.25f, 1}; break;
            default:                           bg = {0.36f, 0.28f, 0.10f, 1}; break;
        }
        DRAWBOT_PathRef path = NULL; DRAWBOT_BrushRef brush = NULL;
        ERR(suites.supplier_suiteP->NewPath(sup, &path));
        ERR(suites.supplier_suiteP->NewBrush(sup, &bg, &brush));
        ERR(suites.path_suiteP->AddRect(path, &r));
        ERR(suites.surface_suiteP->FillPath(surf, brush, path, kDRAWBOT_FillType_Default));
        char line1[300], line2[300];
        if (arb.space[0])
            snprintf(line1, sizeof(line1), (arb.direction == MINC_DIR_TO_WORKING) ? "%s -> working" : "working -> %s", arb.space);
        else snprintf(line1, sizeof(line1), "(unset - name the effect and Sync)");
        switch (status) {
            case MINC_STATUS_OK:                snprintf(line2, sizeof(line2), viaPassport ? "OK (passport)  \xc2\xb7  working: %s" : "OK  \xc2\xb7  working: %s", auth.workingSpace); break;
            case MINC_STATUS_PASS_EMPTY:        snprintf(line2, sizeof(line2), "passthrough"); break;
            case MINC_STATUS_PASS_OCIO_OFF:     snprintf(line2, sizeof(line2), "PASSTHROUGH: project not in OCIO mode"); break;
            case MINC_STATUS_PASS_UNKNOWN_SPACE:snprintf(line2, sizeof(line2), "PASSTHROUGH: space not in config"); break;
            default:                            snprintf(line2, sizeof(line2), "PASSTHROUGH: config unreadable"); break;
        }
        float fsize = 0; DRAWBOT_FontRef font = NULL; DRAWBOT_BrushRef white = NULL;
        ERR(suites.supplier_suiteP->GetDefaultFontSize(sup, &fsize));
        ERR(suites.supplier_suiteP->NewDefaultFont(sup, fsize, &font));
        DRAWBOT_ColorRGBA wcol = {0.92f, 0.92f, 0.92f, 1};
        ERR(suites.supplier_suiteP->NewBrush(sup, &wcol, &white));
        DRAWBOT_UTF16Char u16[300]; DRAWBOT_PointF32 o;
        Ascii16(line1, u16, 300); o.x = fr.left + 6.0f; o.y = fr.top + 13.0f;
        ERR(suites.surface_suiteP->DrawString(surf, white, font, u16, &o, kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_None, 0.0f));
        Ascii16(line2, u16, 300); o.y = fr.top + 26.0f;
        ERR(suites.surface_suiteP->DrawString(surf, white, font, u16, &o, kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_None, 0.0f));
        if (white) ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)white));
        if (font)  ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)font));
        if (brush) ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)brush));
        if (path)  ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)path));
        extra->evt_out_flags = PF_EO_HANDLED_EVENT;
    }
    ERR2(AEFX_ReleaseDrawbotSuites(in_data, out_data));
    return err;
}
