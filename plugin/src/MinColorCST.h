/*  minColorCST — the delegation effect.
    Stores ONLY a colorspace name + direction; resolves the project's CURRENT OCIO config and
    working space at render time (AEGP_ColorSettingsSuite5 snapshot) and processes with our own
    statically linked OCIO 2.5. Late binding by construction: no stored path, no stale snapshot,
    no ValidateOCIOColorSpace abort. See minColor PREPLAN section 6.                          */
#pragma once

#define PF_TABLE_BITS 12
#define PF_TABLE_SZ_16 4096
#define PF_DEEP_COLOR_AWARE 1

#include "AEConfig.h"
#ifdef AE_OS_WIN
#include <Windows.h>
#endif
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "AE_GeneralPlug.h"
#include "AEGP_SuiteHandler.h"

#define MINC_NAME          "minColor CST"
#define MINC_MATCH_NAME    "MINC CST"
#define MINC_CATEGORY      "minColor"
#define MINC_MAJOR_VERSION 1
#define MINC_MINOR_VERSION 0
#define MINC_BUG_VERSION   0
#define MINC_STAGE_VERSION PF_Stage_DEVELOP
#define MINC_BUILD_VERSION 1

enum { MINC_INPUT = 0, MINC_ARB, MINC_NUM_PARAMS };
enum { ARB_DISK_ID = 1 };

#include "MincTypes.h"

/* Arb.cpp */
PF_Err MincHandleArbitrary(PF_InData *in_data, PF_OutData *out_data,
                           PF_ParamDef *params[], PF_LayerDef *output,
                           PF_ArbParamsExtra *extra);
PF_Err MincArbNewDefault(PF_InData *in_data, PF_ArbitraryH *arbPH);

void MincDebugLog(const char *fmt, ...);                 /* Authority.cpp; debug builds of M1/M2 */

/* Authority.cpp — main-thread snapshot of AE's current OCIO state */
void   MincAuthorityGlobalSetup(PF_InData *in_data);   /* RegisterWithAEGP + hooks (idempotent) */
void   MincAuthorityRefresh(PF_InData *in_data);       /* main-thread only; no-ops elsewhere    */
bool   MincAuthorityGet(MincAuthoritySnapshot *out);   /* thread-safe copy                      */

/* SmartRender.cpp */
PF_Err MincSmartPreRender(PF_InData *in_data, PF_OutData *out_data, PF_PreRenderExtra *extra);
PF_Err MincSmartRender(PF_InData *in_data, PF_OutData *out_data, PF_SmartRenderExtra *extra);

extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                                       PF_ParamDef *params[], PF_LayerDef *output, void *extra);
