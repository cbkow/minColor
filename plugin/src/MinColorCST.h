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

/* identity strings + version ints come from core/MincIds.h (single source shared with the
   five PiPLs, pulled in via MincCore.h -> MincTypes.h). Only the stage macro is C-only. */
#define MINC_STAGE_VERSION PF_Stage_DEVELOP

enum { MINC_INPUT = 0, MINC_ARB, MINC_SERIAL, MINC_NUM_PARAMS };
enum { ARB_DISK_ID = 1, SERIAL_DISK_ID = 2 };

#include "MincCore.h"       /* shared core: types (incl. MincIds.h identities + MincVerb), log,
                               utf16, authority, mint, walk */

/* Arb.cpp */
PF_Err MincHandleArbitrary(PF_InData *in_data, PF_OutData *out_data,
                           PF_ParamDef *params[], PF_LayerDef *output,
                           PF_ArbParamsExtra *extra);
PF_Err MincArbNewDefault(PF_InData *in_data, PF_ArbitraryH *arbPH);

/* EffectAuthority.cpp — effect-binary AEGP id + refresh-only idle hook (2.0: the menu command
   and the generation-watch auto-walk live in the minColorAEGP bundle) */
void   MincAuthorityGlobalSetup(PF_InData *in_data);   /* RegisterWithAEGP + refresh idle hook (idempotent) */
void   MincAuthorityRefresh(PF_InData *in_data);       /* main-thread only; no-ops elsewhere    */

/* SmartRender.cpp */
void MincResolveSeq(PF_InData *in_data, MincSeqData *sd);   /* full resolve: seq clone + registry + param arb */
void MincResolveArb(PF_InData *in_data, MinColorArb *arb);  /* thin wrapper over MincResolveSeq */
/* EffectAuthority.cpp — the reserved badge-edit id */
AEGP_PluginID MincEffectAegpId(void);
bool          MincEffectRegistered(void);

/* EffectEdit.cpp — badge-edit transaction: rename + eager mint + targeted CallGeneric + marker */
bool MincApplyEdit(PF_InData *in_data, int verbI, const char *space);

/* UiMenuMac.mm / Ui.cpp(win) — blocking native popup, returns picked index or -1 */
extern "C" int MincShowMenu(const char *const *items, int n, int checkedIdx);

/* Ui.cpp — the wrapper passes the registered effect's verb (badge menu is per-verb) */
PF_Err MincHandleEvent(MincVerb verb, PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_EventExtra *extra);

PF_Err MincSmartPreRender(PF_InData *in_data, PF_OutData *out_data, PF_PreRenderExtra *extra);
PF_Err MincSmartRender(PF_InData *in_data, PF_OutData *out_data, PF_SmartRenderExtra *extra);

/* five registered effects, one shared implementation — the entry wrapper passes the verb
   statically (MincVerb is in-process only; AE's stored match name is the durable form) */
extern "C" DllExport PF_Err EffectMain      (PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                                             PF_ParamDef *params[], PF_LayerDef *output, void *extra);
extern "C" DllExport PF_Err EffectMainXform (PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                                             PF_ParamDef *params[], PF_LayerDef *output, void *extra);
extern "C" DllExport PF_Err EffectMainView  (PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                                             PF_ParamDef *params[], PF_LayerDef *output, void *extra);
extern "C" DllExport PF_Err EffectMainRender(PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                                             PF_ParamDef *params[], PF_LayerDef *output, void *extra);
extern "C" DllExport PF_Err EffectMainLook  (PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                                             PF_ParamDef *params[], PF_LayerDef *output, void *extra);
