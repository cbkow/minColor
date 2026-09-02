/* Per-binary diagnostic log. MINC_LOG_BASENAME is a per-target compile definition so the two
   bundles never interleave appends in one file (effect: minColorCST_authority, AEGP:
   minColorAEGP). Unconditional MincLog is the M1-era diagnostic; MincDebugLog compiles out.   */
#include "MincCore.h"
#include <cstdio>
#include <cstdarg>

#ifndef MINC_LOG_BASENAME
#define MINC_LOG_BASENAME "minColorCST_authority"
#endif

#ifdef AE_OS_WIN
#include <cstdlib>
#include <chrono>
const char *MincLogPath(void) {
    static char p[MAX_PATH + 40] = "";
    if (!p[0]) { const char *t = getenv("TEMP"); snprintf(p, sizeof(p), "%s/" MINC_LOG_BASENAME ".log", t ? t : "C:/Windows/Temp"); }
    return p;
}
double MincNowMs(void) { return std::chrono::duration<double, std::milli>(std::chrono::system_clock::now().time_since_epoch()).count(); }
static struct MincLoadStampT {
    MincLoadStampT() { FILE *f = fopen(MincLogPath(), "a"); if (f) { fprintf(f, "boot: dll loaded @%.0fms\n", MincNowMs()); fclose(f); } }
} s_mincLoadStamp;
#else
#include <sys/time.h>
const char *MincLogPath(void) { return "/tmp/" MINC_LOG_BASENAME ".log"; }
double MincNowMs(void) { struct timeval tv; gettimeofday(&tv, nullptr); return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0; }
__attribute__((constructor)) static void MincLoadStamp() {          /* dylib load moment: catches static-init cost */
    FILE *f = fopen(MincLogPath(), "a");
    if (f) { fprintf(f, "boot: dylib loaded @%.0fms (%s)\n", MincNowMs(), MINC_BUILD_STAMP); fclose(f); }
}
#endif

void MincDebugLog(const char *fmt, ...) {
#ifndef MINC_DEBUG
    (void)fmt; return;                                   /* hot paths call this per frame/resolve */
#else
    FILE *f = fopen(MincLogPath(), "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
#endif
}

/* M1 diagnostic log — remove once the authority path is proven */
void MincLog(const char *fmt, ...) {
    FILE *f = fopen(MincLogPath(), "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}
