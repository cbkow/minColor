#include "MincSettings.h"
#include "MincCore.h"
#include "MincMenus.h"
#include "MincFs.h"
#include "MincEmbeddedMeta.h"
#include <cstdio>
#include <sys/stat.h>

static bool EnsureDir(const std::string &p) {
    mfs::mkdirs(p);
    return mfs::exists(p);
}

std::string MincSettingsDir(void) {
    static std::string dir;
    if (dir.empty()) {
        std::string d = MincSharedSettingsDir();   /* platform-aware (core): /Users/Shared vs ProgramData */
        EnsureDir(d);                              /* mkdirs creates the minColor parent too */
        EnsureDir(d + "/reports");
        dir = d;
    }
    return dir;
}

bool MincQuietMode(void) {
    struct stat st;
    return stat((MincSettingsDir() + "/quiet-mode").c_str(), &st) == 0;
}

bool MincWriteTextFile(const std::string &path, const std::string &content) {
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(content.data(), 1, content.size(), f);
    fclose(f);
    return true;
}

std::string MincReadTextFile(const std::string &path) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return std::string();
    std::string s;
    char buf[8192]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
}

/* Seed the shell's disk JSON from the AEGP's EMBEDDED copies. The shell is ExtendScript and can't
   read the binary's embedded store, so the AEGP writes what the shell reads (preset labels, render
   recipes, extension rules) into the settings dir at launch — the installer ships none of it. Seed-
   if-absent: never clobber a user's edited extension-defaults.json (the shell writes that one). */
void MincSeedSettings(void) {
    const char *files[] = { "presets.json", "render-presets.json", "extension-defaults.json" };
    for (const char *f : files) {
        std::string path = MincSettingsDir() + "/" + f;
        if (mfs::exists(path)) continue;
        std::string data = MincEmbeddedMetaText(f);
        if (!data.empty() && MincWriteTextFile(path, data))
            MincLog("seed: %s written from embedded", f);
    }
}

bool MincWriteReport(const char *ceremony, const std::string &json) {
    return MincWriteTextFile(MincSettingsDir() + "/reports/" + ceremony + "-last.json", json);
}

static std::string JsonStr(const std::string &s) {
    std::string o = "\"";
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    o += "\"";
    return o;
}

bool MincWriteHandshake(const char *const *commandLabels, int n) {
    std::string j = "{\n \"apiVersion\": 1,\n \"version\": " + JsonStr(MINC_VERSION_STR) +
                    ",\n \"buildStamp\": " + JsonStr(MINC_BUILD_STAMP) + ",\n \"commands\": [";
    for (int i = 0; i < n; ++i) {
        if (i) j += ", ";
        j += JsonStr(commandLabels[i]);
    }
    j += "]\n}\n";
    bool ok = MincWriteTextFile(MincSettingsDir() + "/aegp-api.json", j);
    MincLog("handshake: aegp-api.json %s (%d commands)", ok ? "written" : "WRITE FAILED", n);
    return ok;
}
