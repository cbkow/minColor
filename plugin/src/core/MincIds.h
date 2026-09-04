/* Single source for every identity string and version number that must agree between
   MinColorCST.h, MinColorCST_PiPL.r and Info.plist-adjacent tooling. PURE #defines only:
   this file is preprocessed by Rez (macOS) and cl /EP (Windows PiPL pipeline) as well as
   the C++ compiler — no C constructs, no expansion of AE-header macros.                  */
#pragma once

#define MINC_CATEGORY        "minColor"

/* the four variant effects (2.0 timeline-as-UI): verb authority lives in the match name,
   the display name carries the space */
#define MINC_NAME_XFORM      "minColor Transform"
#define MINC_MATCH_XFORM     "MINC XFORM"
#define MINC_ENTRY_XFORM     "EffectMainXform"

#define MINC_NAME_VIEW       "minColor View"
#define MINC_MATCH_VIEW      "MINC VIEW"
#define MINC_ENTRY_VIEW      "EffectMainView"

#define MINC_NAME_RENDER     "minColor Render"
#define MINC_MATCH_RENDER    "MINC RENDER"
#define MINC_ENTRY_RENDER    "EffectMainRender"

#define MINC_NAME_LOOK       "minColor Look"
#define MINC_MATCH_LOOK      "MINC LOOK"
#define MINC_ENTRY_LOOK      "EffectMainLook"

/* the legacy single effect — STILL REGISTERED through M2 (all verbs, full name grammar;
   the 1.4.0 revert taught us retiring it under a live panel breaks every flow). Retirement
   is M3; the match name is kept recognizable forever for clear/migrate.                   */
#define MINC_NAME_LEGACY     "minColor CST"
#define MINC_MATCH_LEGACY    "MINC CST"
#define MINC_ENTRY_LEGACY    "EffectMain"

#define MINC_MAJOR_VERSION   2
#define MINC_MINOR_VERSION   0
#define MINC_BUG_VERSION     0
#define MINC_BUILD_VERSION   2      /* lean-v3: bumped so AE re-scans after dropping CUSTOM_UI */

/* PF_VERSION(2,0,0,PF_Stage_DEVELOP,2) — AE warns "version mismatch" per effect when the
   PiPL value drifts from GLOBAL_SETUP's my_version.                                       */
#define MINC_PIPL_VERSION_HEX 0x00100002

/* mirrors of GlobalSetup()'s out_flags / out_flags2 (MinColorCST.cpp) — AE compares these
   at scan time and warns per effect on mismatch. All five PiPLs share one GlobalSetup.
   lean-v3: CUSTOM_UI (0x8000) dropped — the bespoke badge is retired for the native popup. */
#define MINC_PIPL_OUTFLAGS    0x06000410
#define MINC_PIPL_OUTFLAGS2   0x08A01400
