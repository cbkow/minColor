#include "MincArgs.h"
#include "MincSettings.h"
#include "MincCore.h"
#include <cstdio>

MincJsonPtr MincArgsConsume(const char *commandLabel) {
    std::string p = MincSettingsDir() + "/shell-args.json";
    MincJsonPtr j = MincJsonParseFile(p);
    if (!j) return nullptr;
    if (!commandLabel || j->str("command") != commandLabel) return nullptr;   /* stale/foreign: leave it */
    remove(p.c_str());                                    /* consumed — one dispatch per write */
    MincLog("args: consumed for '%s'", commandLabel);
    return j;
}
