#include "MincArgs.h"
#include "MincSettings.h"
#include "MincCore.h"
#include <cstdio>

static bool g_silent = false;

MincJsonPtr MincArgsConsume(const char *commandLabel) {
    std::string p = MincSettingsDir() + "/shell-args.json";
    MincJsonPtr j = MincJsonParseFile(p);
    if (!j) return nullptr;
    if (!commandLabel || j->str("command") != commandLabel) return nullptr;   /* stale/foreign: leave it */
    remove(p.c_str());                                    /* consumed — one dispatch per write */
    {   MincJsonPtr sil = j->get("silent");
        g_silent = (sil && sil->type == MincJsonValue::Bool && sil->boolV);
    }
    MincLog("args: consumed for '%s'%s", commandLabel, g_silent ? " (silent)" : "");
    return j;
}

bool MincArgsTakeSilent(void)  { bool s = g_silent; g_silent = false; return s; }
void MincArgsResetSilent(void) { g_silent = false; }
