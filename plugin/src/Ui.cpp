/* ECW badge: direction + colorspace + live status chip, drawn with Drawbot. */
#include "MinColorCST.h"
#include "MincMenus.h"      /* per-verb badge menu choices (AEGP-written plugin-menus.json) */
#include "AEFX_SuiteHelper.h"
#include <cstring>
#include <cstdio>
#include <ctime>

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

/* M2 Step 0 — DO_CLICK diagnosis (plan P6): 1.4.0's "clicks never reached the plugin" was
   unproven because the handler had zero logging. Log EVERY event's type+area at entry so
   "AE never sends it" and "the handler bailed" become distinguishable in the log. No outflag
   or behavior changes — DRAW path and PiPL hexes untouched.                                */
static const char *EvtName(PF_EventType t) {
    switch (t) {
        case PF_Event_NEW_CONTEXT:   return "NEW_CONTEXT";
        case PF_Event_ACTIVATE:      return "ACTIVATE";
        case PF_Event_DO_CLICK:      return "DO_CLICK";
        case PF_Event_DRAG:          return "DRAG";
        case PF_Event_DRAW:          return "DRAW";
        case PF_Event_DEACTIVATE:    return "DEACTIVATE";
        case PF_Event_CLOSE_CONTEXT: return "CLOSE_CONTEXT";
        case PF_Event_IDLE:          return "IDLE";
        case PF_Event_ADJUST_CURSOR: return "ADJUST_CURSOR";
        case PF_Event_KEYDOWN:       return "KEYDOWN";
        case PF_Event_MOUSE_EXITED:  return "MOUSE_EXITED";
        default:                     return "?";
    }
}

#ifdef AE_OS_WIN
/* platform popup for the badge menu (mac twin: UiMenuMac.mm). Blocking, returns picked index or -1. */
extern "C" int MincShowMenu(const char *const *items, int n, int checkedIdx) {
    HMENU m = CreatePopupMenu();
    if (!m) return -1;
    for (int i = 0; i < n; ++i) {
        wchar_t w[256];
        if (!MultiByteToWideChar(CP_UTF8, 0, items[i], -1, w, 256)) continue;
        AppendMenuW(m, MF_STRING | (i == checkedIdx ? MF_CHECKED : 0), (UINT_PTR)(i + 1), w);
    }
    POINT pt; GetCursorPos(&pt);
    HWND hwnd = GetActiveWindow(); if (!hwnd) hwnd = GetForegroundWindow();
    int r = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(m);
    return r - 1;
}
#endif

static PF_Err HandleBadgeClick(MincVerb verb, PF_InData *in_data, PF_OutData *out_data, PF_EventExtra *extra) {
    /* per-verb choices: the AEGP-written menus file (curated), else the effective config */
    MincSeqData sd; MincResolveSeq(in_data, &sd);
    MincAuthoritySnapshot live = {}; MincAuthorityGet(&live);
    MincAuthoritySnapshot auth = {};
    MincEffectiveAuthority(&live, &sd, &auth);
    static char items[MINC_MENU_MAX][MINC_SPACE_LEN];        /* events are main-thread only */
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
    if (n == 0) {                                            /* 1.4.0's silent bail, now on the record */
        MincLog("badge: nothing to offer (no menus file, enumeration empty) — click unhandled");
        return PF_Err_NONE;
    }
    const char *ptrs[MINC_MENU_MAX]; int checked = -1;
    for (int i = 0; i < n; ++i) { ptrs[i] = items[i]; if (!strcmp(items[i], sd.arb.space)) checked = i; }
    MincLog("badge: menu open (%d items, checked=%d)", n, checked);
    int pick = MincShowMenu(ptrs, n, checked);
    MincLog("badge: pick=%d%s", pick, pick < 0 ? " (dismissed)" : "");
    if (pick >= 0 && pick < n && strcmp(items[pick], sd.arb.space) != 0) {
        if (MincApplyEdit(in_data, (int)verb, items[pick])) { /* rename + eager-mint payload + marker */
            out_data->out_flags |= PF_OutFlag_REFRESH_UI;    /* repaint the badge NOW — without this the
                                                                chip showed the old space until the next
                                                                unrelated param event (Chris, step 5 test) */
            /* Re-render the FRAME. FORCE_RERENDER is only a "maybe" for smart effects — AE
               re-checks the cache GUID, and our arb change lives in the process-global registry
               (not the flattened seq-data AE fingerprints), so AE decides nothing changed and
               serves the cached frame (Chris: had to scrub a frame away and back). PF_TouchActiveItem
               marks the active item dirty directly — no GUID re-check, no playhead nudge — so the
               current frame actually re-renders; GuidMix then keys the cache on the new arb.       */
            out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
            PF_AdvItemSuite1 *adv = NULL;
            if (!AEFX_AcquireSuite(in_data, out_data, kPFAdvItemSuite, kPFAdvItemSuiteVersion1, NULL, (void **)&adv) && adv) {
                adv->PF_TouchActiveItem();
                AEFX_ReleaseSuite(in_data, out_data, kPFAdvItemSuite, kPFAdvItemSuiteVersion1, NULL);
            }
        }
    }
    extra->u.do_click.send_drag = FALSE;
    extra->evt_out_flags = PF_EO_HANDLED_EVENT;
    return PF_Err_NONE;
}

PF_Err MincHandleEvent(MincVerb verb, PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_EventExtra *extra) {
    PF_Err err = PF_Err_NONE, err2 = PF_Err_NONE;
    {   /* diagnosis logging: DRAW once per session, ADJUST_CURSOR at most 1/s (mouse-move
           spam), everything else every occurrence with click coordinates when present */
        static bool drawLogged = false;
        static long lastCursorS = 0;
        if (extra->e_type == PF_Event_DRAW) {
            if (!drawLogged) { drawLogged = true; MincLog("event: DRAW area=%d (logged once)", (int)extra->effect_win.area); }
        } else if (extra->e_type == PF_Event_ADJUST_CURSOR) {
            long nowS = (long)time(nullptr);
            if (nowS != lastCursorS) {
                lastCursorS = nowS;
                MincLog("event: ADJUST_CURSOR area=%d", (int)extra->effect_win.area);
            }
        } else if (extra->e_type == PF_Event_DO_CLICK || extra->e_type == PF_Event_DRAG) {
            MincLog("event: %s area=%d screen=(%d,%d)", EvtName(extra->e_type),
                    (int)extra->effect_win.area,
                    (int)extra->u.do_click.screen_point.h, (int)extra->u.do_click.screen_point.v);
        } else {
            MincLog("event: %s area=%d", EvtName(extra->e_type), (int)extra->effect_win.area);
        }
    }
    if (extra->e_type == PF_Event_DO_CLICK && extra->effect_win.area == PF_EA_CONTROL &&
        verb != MINC_VERB_LEGACY)                            /* legacy badge display-only — Windows-only
                                                                code from M3 step 8 (mac has no legacy) */
        return HandleBadgeClick(verb, in_data, out_data, extra);
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
        /* Badge as a labeled DROPDOWN + a status row (2026-09-03, Chris's design):
           the direction (→working / working→) is fixed by which effect this IS, never a user
           choice, so it becomes the LABEL, not part of the value. Top row = verb label +
           neutral dropdown field showing the SPACE only + chevron. Bottom row = a colored
           status dot + short health text (color lives only on the dot — a calm timeline reads
           neutral, a problem stands out). Legacy is display-only: label + space, no field.   */
        const PF_Rect fr = extra->effect_win.current_frame;
        const float L = (float)fr.left, T = (float)fr.top, R = (float)fr.right;
        bool clickable = (verb != MINC_VERB_LEGACY);

        /* neutral panel background (status no longer colors the whole block) */
        DRAWBOT_ColorRGBA bg = {0.16f, 0.16f, 0.17f, 1};
        {   DRAWBOT_RectF32 r = { L + 0.5f, T + 0.5f, (float)(fr.right - fr.left), (float)(fr.bottom - fr.top) };
            DRAWBOT_PathRef p = NULL; DRAWBOT_BrushRef b = NULL;
            ERR(suites.supplier_suiteP->NewPath(sup, &p));
            ERR(suites.supplier_suiteP->NewBrush(sup, &bg, &b));
            ERR(suites.path_suiteP->AddRect(p, &r));
            ERR(suites.surface_suiteP->FillPath(surf, b, p, kDRAWBOT_FillType_Default));
            if (b) ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)b));
            if (p) ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)p));
        }

        const char *label;
        switch (verb) {
            case MINC_VERB_LOOK:   label = "Look";   break;
            case MINC_VERB_VIEW:   label = "View";   break;
            case MINC_VERB_RENDER: label = "Render"; break;
            case MINC_VERB_XFORM:  label = "Input";  break;   /* input or contain: both → working */
            default: label = (arb.direction == MINC_DIR_LOOK) ? "Look"
                            : (arb.direction == MINC_DIR_FROM_WORKING) ? "View" : "Input"; break;
        }
        char value[MINC_SPACE_LEN + 8];
        if (arb.space[0]) snprintf(value, sizeof(value), "%s", arb.space);
        else              snprintf(value, sizeof(value), "(unset)");

        float fsize = 0; DRAWBOT_FontRef font = NULL;
        ERR(suites.supplier_suiteP->GetDefaultFontSize(sup, &fsize));
        ERR(suites.supplier_suiteP->NewDefaultFont(sup, fsize, &font));
        DRAWBOT_ColorRGBA wcol = {0.92f, 0.92f, 0.92f, 1}, dim = {0.62f, 0.62f, 0.64f, 1};
        DRAWBOT_BrushRef white = NULL, grey = NULL;
        ERR(suites.supplier_suiteP->NewBrush(sup, &wcol, &white));
        ERR(suites.supplier_suiteP->NewBrush(sup, &dim, &grey));
        DRAWBOT_UTF16Char u16[300]; DRAWBOT_PointF32 o;

        /* --- row 1: label + dropdown field --- */
        const float fieldL = L + 52.0f, fieldR = R - 6.0f, rowY = T + 13.0f;
        Ascii16(label, u16, 300); o.x = L + 6.0f; o.y = rowY;
        ERR(suites.surface_suiteP->DrawString(surf, grey, font, u16, &o, kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_None, 0.0f));
        if (clickable) {
            DRAWBOT_ColorRGBA fieldbg = {0.22f, 0.22f, 0.24f, 1}, edge = {0.92f, 0.92f, 0.92f, 0.30f};
            DRAWBOT_RectF32 fld = { fieldL, T + 3.0f, fieldR - fieldL, 16.0f };
            DRAWBOT_PathRef fp = NULL; DRAWBOT_BrushRef fb = NULL; DRAWBOT_PenRef fpen = NULL;
            if (suites.supplier_suiteP->NewBrush(sup, &fieldbg, &fb) == kSPNoError && fb &&
                suites.supplier_suiteP->NewPath(sup, &fp) == kSPNoError && fp) {
                suites.path_suiteP->AddRect(fp, &fld);
                suites.surface_suiteP->FillPath(surf, fb, fp, kDRAWBOT_FillType_Default);
                if (suites.supplier_suiteP->NewPen(sup, &edge, 1.0f, &fpen) == kSPNoError && fpen) {
                    suites.surface_suiteP->StrokePath(surf, fpen, fp);
                    ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)fpen));
                }
                ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)fp));
            }
            if (fb) ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)fb));
            /* chevron at field right edge */
            float cx = fieldR - 9.0f, cy = T + 8.0f;
            DRAWBOT_PenRef cpen = NULL;
            if (suites.supplier_suiteP->NewPen(sup, &wcol, 1.4f, &cpen) == kSPNoError && cpen) {
                DRAWBOT_PathRef cpath = NULL;
                if (suites.supplier_suiteP->NewPath(sup, &cpath) == kSPNoError && cpath) {
                    suites.path_suiteP->MoveTo(cpath, cx - 3.5f, cy);
                    suites.path_suiteP->LineTo(cpath, cx, cy + 3.5f);
                    suites.path_suiteP->LineTo(cpath, cx + 3.5f, cy);
                    suites.surface_suiteP->StrokePath(surf, cpen, cpath);
                    ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)cpath));
                }
                ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)cpen));
            }
        }
        Ascii16(value, u16, 300); o.x = fieldL + 4.0f; o.y = rowY;
        ERR(suites.surface_suiteP->DrawString(surf, white, font, u16, &o, kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_None, 0.0f));

        /* --- row 2: status dot + short health text --- */
        DRAWBOT_ColorRGBA dot; const char *stext;
        switch (status) {
            case MINC_STATUS_OK:                 dot = viaPassport ? DRAWBOT_ColorRGBA{0.32f, 0.60f, 0.95f, 1}
                                                                   : DRAWBOT_ColorRGBA{0.30f, 0.82f, 0.42f, 1};
                                                 stext = viaPassport ? "via passport" : "OK"; break;
            case MINC_STATUS_PASS_EMPTY:         dot = {0.55f, 0.55f, 0.55f, 1}; stext = "passthrough"; break;
            case MINC_STATUS_PASS_OCIO_OFF:      dot = {0.95f, 0.72f, 0.20f, 1}; stext = "passthrough \xe2\x80\x94 not in OCIO"; break;
            case MINC_STATUS_PASS_UNKNOWN_SPACE: dot = {0.95f, 0.72f, 0.20f, 1}; stext = "passthrough \xe2\x80\x94 space not in config"; break;
            default:                             dot = {0.95f, 0.72f, 0.20f, 1}; stext = "passthrough \xe2\x80\x94 config unreadable"; break;
        }
        {   DRAWBOT_RectF32 d = { L + 6.0f, T + 21.0f, 6.0f, 6.0f };
            DRAWBOT_PathRef dp = NULL; DRAWBOT_BrushRef db = NULL;
            if (suites.supplier_suiteP->NewBrush(sup, &dot, &db) == kSPNoError && db &&
                suites.supplier_suiteP->NewPath(sup, &dp) == kSPNoError && dp) {
                suites.path_suiteP->AddRect(dp, &d);
                suites.surface_suiteP->FillPath(surf, db, dp, kDRAWBOT_FillType_Default);
                ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)dp));
            }
            if (db) ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)db));
        }
        Ascii16(stext, u16, 300); o.x = L + 16.0f; o.y = T + 27.0f;
        ERR(suites.surface_suiteP->DrawString(surf, grey, font, u16, &o, kDRAWBOT_TextAlignment_Default, kDRAWBOT_TextTruncation_None, 0.0f));

        if (white) ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)white));
        if (grey)  ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)grey));
        if (font)  ERR2(suites.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)font));
        extra->evt_out_flags = PF_EO_HANDLED_EVENT;
    }
    ERR2(AEFX_ReleaseDrawbotSuites(in_data, out_data));
    return err;
}
