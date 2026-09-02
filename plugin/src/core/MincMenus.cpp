#include "MincMenus.h"
#include "MincCore.h"
#include "MincJson.h"
#include <string>
#include <sys/stat.h>
#include <cstring>

const char *MincSharedSettingsDir(void) {
#ifdef AE_OS_WIN
    return "C:/ProgramData/minColor/settings";
#else
    return "/Users/Shared/minColor/settings";
#endif
}

static void CopyField(char *dst, size_t cap, const std::string &s) {
    strncpy(dst, s.c_str(), cap - 1);
    dst[cap - 1] = 0;
}
static int CopyList(char dst[][MINC_SPACE_LEN], MincJsonPtr node, const char *key) {
    int n = 0;
    if (!node) return 0;
    MincJsonPtr a = node->get(key);
    if (!a || a->type != MincJsonValue::Array) return 0;
    for (auto &e : a->arr) {
        if (n >= MINC_MENU_MAX) break;
        if (e && e->type == MincJsonValue::String) CopyField(dst[n++], MINC_SPACE_LEN, e->strV);
    }
    return n;
}

bool MincMenusGet(MincMenus *out) {
    static MincMenus cache = {};
    static time_t lastM = 0;
    static off_t lastSz = -1;
    std::string path = std::string(MincSharedSettingsDir()) + "/plugin-menus.json";
    struct stat st;
    if (stat(path.c_str(), &st) != 0) { cache.valid = false; lastM = 0; lastSz = -1; if (out) *out = cache; return false; }
    if (st.st_mtime != lastM || st.st_size != lastSz) {
        lastM = st.st_mtime; lastSz = st.st_size;
        memset(&cache, 0, sizeof(cache));
        MincJsonPtr j = MincJsonParseFile(path);
        if (j) {
            CopyField(cache.preset, sizeof(cache.preset), j->str("preset"));
            CopyField(cache.family, sizeof(cache.family), j->str("family"));
            CopyField(cache.defaultView, sizeof(cache.defaultView), j->str("defaultView"));
            CopyField(cache.defaultRender, sizeof(cache.defaultRender), j->str("defaultRender"));
            cache.nInput  = CopyList(cache.inputSpaces,  j, "inputSpaces");
            cache.nView   = CopyList(cache.viewSpaces,   j, "viewSpaces");
            cache.nRender = CopyList(cache.renderSpaces, j, "renderSpaces");
            cache.nLooks  = CopyList(cache.looks,        j, "looks");
        }
        /* valid ONLY with both defaults — anything else and callers skip */
        cache.valid = (cache.defaultView[0] != 0 && cache.defaultRender[0] != 0);
    }
    if (out) *out = cache;
    return cache.valid;
}
