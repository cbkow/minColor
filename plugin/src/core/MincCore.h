/* mincCore — shared static library linked into BOTH bundles (minColorCST effect, minColorAEGP).
   Everything here acts through AEGP suites and CallGeneric payloads, so it is location-
   independent: CallGeneric always dispatches into the EFFECT binary's HandleGeneric/registry
   no matter which binary calls. Statics in core (.cpp files) are PER-BINARY by design — each
   bundle keeps its own authority snapshot, log file, and mint counter.                        */
#pragma once

#include "AEConfig.h"
#ifdef AE_OS_WIN
#include <Windows.h>
#endif
#include "entry.h"
#include "AE_Effect.h"
#include "AE_GeneralPlug.h"
#include "AEGP_SuiteHandler.h"
#include "MincTypes.h"
#include "MincBuildStamp.h"
#include "MincSignal.h"     /* cross-bundle walk marker (effect arms, AEGP consumes) */
#include <set>

/* ---- MincLog.cpp — per-binary log file, basename via MINC_LOG_BASENAME compile def ---- */
void        MincDebugLog(const char *fmt, ...);          /* compiled out unless MINC_DEBUG */
void        MincLog(const char *fmt, ...);               /* unconditional diagnostic log */
double      MincNowMs(void);
const char *MincLogPath(void);

/* ---- MincUtf16.cpp ---- */
void MincUtf16HandleToUtf8(AEGP_SuiteHandler &suites, AEGP_MemHandle h, char *out, size_t outLen);
void MincU8ToU16(const char *s, A_UTF16Char *out, int cap);   /* BMP-only, mirrors the reader */

/* ---- MincAuthority.cpp — snapshot of AE's current OCIO state (per-binary instance) ---- */
void MincSetMainThread(void);                            /* call once from the binary's entry (main thread) */
void MincAuthorityRefreshBp(SPBasicSuite *bp, AEGP_PluginID aegpId);   /* main-thread only; no-ops elsewhere */
bool MincAuthorityGet(MincAuthoritySnapshot *out);       /* thread-safe copy */

/* ---- MincMint.cpp ---- */
uint32_t MincMintInstanceId(void);

/* ---- MincSyncWalk.cpp — names are the durable store; the walk re-derives every instance ---- */
/* christen=true additionally names default-named fresh VIEW/RENDER variants from the
   plugin-menus.json defaults (never invents a space — no menus file, no christening) */
void MincSyncFromNames(SPBasicSuite *bp, AEGP_PluginID aegpId, bool christen = false);
bool MincParseGrammarVerb(MincVerb verb, const char *utf8, MinColorArb *out);
/* The blessed arb-write transport (factored from the walk): write one effect instance's arb via
   AEGP_EffectCallGeneric -> HandleGeneric (param + seq + registry), minting/adopting an instance
   id as needed. avoidIds/nameForLog/counters are the walk's bookkeeping (optional — ceremonies
   authoring a single fresh effect pass nullptr and get a plain write). */
void MincWriteEffectArb(SPBasicSuite *bp, AEGP_PluginID aegpId, AEGP_EffectRefH effH,
                        const MinColorArb *arb, const char *configBase, const char *passportWorking,
                        std::set<uint32_t> *avoidIds = nullptr, const char *nameForLog = nullptr,
                        int *wrote = nullptr, int *reminted = nullptr, int *placeholders = nullptr);

/* tiny RAII suite acquire */
template <typename SUITE>
struct Acq {
    SPBasicSuite *pica; const char *name; A_long ver; SUITE *p = nullptr;
    Acq(SPBasicSuite *b, const char *n, A_long v) : pica(b), name(n), ver(v) {
        const void *vp = nullptr;
        if (b->AcquireSuite(n, v, &vp) == kSPNoError) p = (SUITE*)const_cast<void*>(vp);
    }
    ~Acq() { if (p) pica->ReleaseSuite(name, ver); }
    SUITE *operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
};
