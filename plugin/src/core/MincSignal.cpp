#include "MincSignal.h"
#include "MincMenus.h"          /* MincSharedSettingsDir */
#include "MincCore.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

static std::string MarkerPath() { return std::string(MincSharedSettingsDir()) + "/walk-pending"; }

void MincTouchWalkMarker(const char *reason) {
    static std::atomic<long> lastMs{0};
    long now = (long)MincNowMs();
    long prev = lastMs.load();
    bool christen = reason && !strcmp(reason, "christen");
    if (!christen && now - prev < 500) return;           /* paste storms cost one fopen per burst */
    lastMs.store(now);
    std::string p = MarkerPath();
    if (!christen) {                                     /* never downgrade: christen ⊃ sync */
        FILE *r = fopen(p.c_str(), "r");
        if (r) {
            char buf[32] = "";
            if (fgets(buf, sizeof(buf), r) && !strncmp(buf, "christen", 8)) { fclose(r); return; }
            fclose(r);
        }
    }
    FILE *f = fopen(p.c_str(), "w");
    if (f) { fputs(reason ? reason : "sync", f); fclose(f); }
    else {
        static bool warned = false;                      /* settings dir unwritable: signal (and
                                                            christening) silently unavailable */
        if (!warned) { warned = true; MincLog("signal: walk marker unwritable (%s)", p.c_str()); }
    }
}

bool MincConsumeWalkMarker(char *reasonOut, size_t cap) {
    std::string p = MarkerPath();
    FILE *f = fopen(p.c_str(), "r");
    if (!f) return false;
    char buf[32] = "";
    if (!fgets(buf, sizeof(buf), f)) buf[0] = 0;
    fclose(f);
    remove(p.c_str());                                   /* delete FIRST semantics: we hold the content */
    if (reasonOut && cap) { strncpy(reasonOut, buf, cap - 1); reasonOut[cap - 1] = 0; }
    return true;
}
