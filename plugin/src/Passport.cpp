/* Passport resolution — AE-SDK-free.
   The plugin binary LIVES in the central store root (MediaCore/minColor/), so the store is
   found relative to our own image: no hardcoded paths, survives store relocation, and is
   per-platform by construction. When live authority is dead (cross-platform arrival: AE wiped
   the pin and fell back to Adobe mode), the effect's passport (config BASENAME + working space,
   carried in sequence data inside the .aep) is dereferenced against the local store — the
   hashed filename is content identity, so pixels are exact wherever it resolves. */
#include "MincTypes.h"
#include <cstring>
#include <cstdio>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static bool MincPathExists(const char *p) {
    wchar_t w[2048];
    if (!MultiByteToWideChar(CP_UTF8, 0, p, -1, w, 2048)) return false;
    return GetFileAttributesW(w) != INVALID_FILE_ATTRIBUTES;
}
static bool MincOwnImagePath(char *out, size_t cap) {
    HMODULE h = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)&MincOwnImagePath, &h)) return false;
    wchar_t w[2048];
    if (!GetModuleFileNameW(h, w, 2048)) return false;
    return WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, nullptr, nullptr) > 0;
}
#else
#include <dlfcn.h>
#include <sys/stat.h>
static bool MincPathExists(const char *p) { struct stat st; return stat(p, &st) == 0; }
static bool MincOwnImagePath(char *out, size_t cap) {
    Dl_info info;
    if (!dladdr((void*)&MincOwnImagePath, &info) || !info.dli_fname) return false;
    snprintf(out, cap, "%s", info.dli_fname);
    return true;
}
#endif

const char *MincLocalStoreConfigDir(void) {
    static bool resolved = false;
    static char dir[2048] = "";
    if (resolved) return dir;
    resolved = true;
    char img[2048] = "";
    if (!MincOwnImagePath(img, sizeof(img))) return dir;
    std::string p(img);
    for (char &c : p) if (c == '\\') c = '/';
    for (int up = 0; up < 5; ++up) {                 /* mac: binary is 3 levels inside the bundle */
        size_t s = p.find_last_of('/');
        if (s == std::string::npos || s == 0) break;
        p.resize(s);
        std::string cand = p + "/configs";
        if (MincPathExists(cand.c_str())) { snprintf(dir, sizeof(dir), "%s", cand.c_str()); break; }
    }
    return dir;
}

bool MincEffectiveAuthority(const MincAuthoritySnapshot *live, const MincSeqData *sd,
                            MincAuthoritySnapshot *out) {
    /* lean-v3 SELF-CONTAINED: the effect renders from its OWN config — the passport's config
       basename, resolved against the plugin-relative store — NOT AE's project config. AE's
       pinned config is a neutralizer we deliberately ignore (it may be a lean "interface"
       config that lacks the input spaces). The passport WINS whenever its config resolves;
       working space is passportWorking when set, else the engine derives it from the config's
       scene_linear role (embrace-scene_linear). We fall back to AE's live authority only for a
       genuinely unauthored/legacy effect (no passport) or a config the store can't resolve
       (e.g. a user's custom pin) — best effort, never silently wrong for a managed project. */
    bool passportOk = sd && sd->configBase[0] &&
                      !strchr(sd->configBase, '/') && !strchr(sd->configBase, '\\') &&
                      !strstr(sd->configBase, "..");
    const char *store = MincLocalStoreConfigDir();
    if (passportOk && store[0]) {
        char cand[2600];
        snprintf(cand, sizeof(cand), "%s/%s", store, sd->configBase);
        if (MincPathExists(cand)) {
            memset(out, 0, sizeof(*out));
            out->ocioOn = true;
            snprintf(out->configPath, sizeof(out->configPath), "%s", cand);
            snprintf(out->workingSpace, sizeof(out->workingSpace), "%s", sd->passportWorking);
            out->generation = live->generation;
            return true;
        }
    }
    *out = *live;                                    /* no resolvable passport -> AE's live authority */
    return false;
}
