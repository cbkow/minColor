#include "MincSettings.h"
#include "MincCore.h"
#include "MincMenus.h"
#include "MincFs.h"
#include "MincEmbeddedMeta.h"
#include "MincJson.h"
#include <cstdio>
#include <set>
#include <vector>
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
static std::string JsonStr(const std::string &s);

/* extension-defaults.json MERGE (2026-09-10). Seed-if-absent alone never delivered new default
   rows to a machine whose table already existed (the 09-02 container rows reached nobody who
   ran a build before that date). Rule: add an embedded row only when the user's table lacks
   the extension AND the seed has never offered it before; never change a value that exists.
   "seeded" is the history — every extension the seed has ever contributed — so a row the user
   deleted stays deleted on every later launch. Returns true when the file was rewritten.     */
static bool MergeExtensionDefaults(const std::string &path) {
    MincJsonPtr ej = MincJsonParse(MincEmbeddedMetaText("extension-defaults.json"));
    MincJsonPtr ed = ej ? ej->get("defaults") : nullptr;
    if (!ed || ed->type != MincJsonValue::Object) return false;
    MincJsonPtr dj = MincJsonParseFile(path);
    if (!dj) { MincLog("seed: extension-defaults.json unreadable \xe2\x80\x94 left alone"); return false; }
    std::map<std::string, std::string> defaults;             /* sorted on write, like the shell */
    std::set<std::string> seeded;
    MincJsonPtr dd = dj->get("defaults");
    if (dd && dd->type == MincJsonValue::Object)
        for (auto &kv : dd->obj)
            if (kv.second && kv.second->type == MincJsonValue::String) defaults[kv.first] = kv.second->strV;
    MincJsonPtr ds = dj->get("seeded");
    if (ds && ds->type == MincJsonValue::Array)
        for (auto &v : ds->arr)
            if (v && v->type == MincJsonValue::String) seeded.insert(v->strV);
    std::vector<std::string> added;
    bool historyGrew = false;
    for (auto &kv : ed->obj) {
        if (!kv.second || kv.second->type != MincJsonValue::String) continue;
        if (seeded.count(kv.first)) continue;                /* offered before: the user's call stands */
        seeded.insert(kv.first); historyGrew = true;
        if (!defaults.count(kv.first)) { defaults[kv.first] = kv.second->strV; added.push_back(kv.first); }
    }
    if (!historyGrew) return false;
    std::string j = "{ \"defaults\": { ";
    bool first = true;
    for (auto &kv : defaults) { if (!first) j += ", "; first = false; j += JsonStr(kv.first) + ": " + JsonStr(kv.second); }
    j += " }, \"seeded\": [";
    first = true;
    for (auto &k : seeded) { if (!first) j += ", "; first = false; j += JsonStr(k); }
    j += "] }\n";
    if (!MincWriteTextFile(path, j)) { MincLog("seed: extension-defaults.json merge WRITE FAILED"); return false; }
    std::string list;
    for (auto &a : added) { if (!list.empty()) list += ", "; list += a; }
    MincLog("seed: extension-defaults.json merged \xe2\x80\x94 added %s", added.empty() ? "nothing (history only)" : list.c_str());
    return true;
}

void MincSeedSettings(void) {
    const char *files[] = { "presets.json", "render-presets.json", "extension-defaults.json" };
    for (const char *f : files) {
        std::string path = MincSettingsDir() + "/" + f;
        if (mfs::exists(path)) {
            if (std::string(f) == "extension-defaults.json") MergeExtensionDefaults(path);
            continue;
        }
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
