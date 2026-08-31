#include "MincSuggest.h"
#include "MincPresets.h"
#include "MincSettings.h"
#include "MincJson.h"
#include <cstdio>
#include <cstring>
#include <cctype>

/* ---- configSpaces (:32-47): names + aliases of a .ocio, "    name: X" / "    aliases: [..]" ---- */
static std::map<std::string, bool> ConfigSpaces(const std::string &path) {
    std::map<std::string, bool> set;
    std::string s = MincReadTextFile(path);
    size_t pos = 0;
    while (pos < s.size()) {
        size_t eol = s.find('\n', pos);
        if (eol == std::string::npos) eol = s.size();
        std::string line = s.substr(pos, eol - pos);
        if (line.compare(0, 10, "    name: ") == 0) {
            std::string n = line.substr(10);
            while (!n.empty() && (n.back() == ' ' || n.back() == '\r')) n.pop_back();
            if (!n.empty()) set[n] = true;
        } else if (line.compare(0, 14, "    aliases: [") == 0) {
            size_t close = line.find(']');
            std::string body = line.substr(14, close == std::string::npos ? std::string::npos : close - 14);
            size_t p2 = 0;
            while (p2 <= body.size()) {
                size_t c = body.find(',', p2);
                std::string a = body.substr(p2, c == std::string::npos ? std::string::npos : c - p2);
                while (!a.empty() && (a.front() == ' ' || a.front() == '"')) a.erase(0, 1);
                while (!a.empty() && (a.back() == ' ' || a.back() == '"' || a.back() == '\r')) a.pop_back();
                if (!a.empty()) set[a] = true;
                if (c == std::string::npos) break;
                p2 = c + 1;
            }
        }
        pos = eol + 1;
    }
    return set;
}

/* ---- legacy maps + family defaults (:859-877; AEGP is mac -> macOS Video View) ---- */
struct KV { const char *k, *v; };
static const KV LEG_LINEAR[] = {
    {"sRGB IEC61966-2.1", "sRGB"}, {"Standard", "Gamma 2.4 Encoded Rec.709"}, {"Rec.1886", "Gamma 2.4 Encoded Rec.709"},
    {"UnionMacOS", "macOS Desktop View"}, {"macOS View Only", "macOS Desktop View"}, {"macOS Desktop View Only", "macOS Desktop View"},
    {"UnionMacOS 1886 Sim", "macOS Video View"}, {"macOS View Only (1886 Sim)", "macOS Video View"}, {"macOS Video View Only", "macOS Video View"},
    {"Windows Desktop View Only", "Windows Desktop View"}, {"Windows Video View Only", "Windows Video View"},
};
static const KV LEG_DISPLAY[] = {
    {"sRGB IEC61966-2.1", "sRGB"}, {"Standard", "working"}, {"Gamma 2.4 Encoded Rec.709", "working"},
    {"Camera Rec.709", "working"}, {"Rec.1886", "working"},
    {"UnionMacOS", "macOS Desktop View"}, {"macOS View Only", "macOS Desktop View"}, {"macOS Desktop View Only", "macOS Desktop View"},
    {"UnionMacOS 1886 Sim", "macOS Video View"}, {"macOS View Only (1886 Sim)", "macOS Video View"}, {"macOS Video View Only", "macOS Video View"},
    {"Windows Desktop View Only", "Windows Desktop View"}, {"Windows Video View Only", "Windows Video View"},
};
static std::string LegacyMap(const std::string &family, const std::string &name) {
    const KV *t = (family == "Display") ? LEG_DISPLAY : LEG_LINEAR;
    int n = (family == "Display") ? (int)(sizeof(LEG_DISPLAY)/sizeof(KV)) : (int)(sizeof(LEG_LINEAR)/sizeof(KV));
    for (int i = 0; i < n; ++i) if (name == t[i].k) return t[i].v;
    return "";
}

/* ---- normalizeSpace (:879-886): canonical | "working" | "" ---- */
static std::string NormalizeSpace(const std::string &name, const std::map<std::string, bool> &valid, const std::string &family) {
    if (name.empty()) return "";
    if (name == "working") return "working";
    if (valid.count(name)) return name;
    std::string m = LegacyMap(family.empty() ? "Linear" : family, name);
    if (m == "working") return "working";
    return (!m.empty() && valid.count(m)) ? m : "";
}

/* ---- menuLists port (:57-73) via presets.json arrays + pin-behind filtering ---- */
static void JsonList(MincJsonPtr node, const char *key, std::vector<std::string> &out) {
    if (!node) return;
    MincJsonPtr a = node->get(key);
    if (!a || a->type != MincJsonValue::Array) return;
    for (auto &v : a->arr) if (v && v->type == MincJsonValue::String) out.push_back(v->strV);
}

static std::vector<std::string> ConfigLooks(const std::string &pinPath) {  /* configLooks port (:1201) */
    std::vector<std::string> names;
    std::string s = MincReadTextFile(pinPath);
    size_t li = s.find("\nlooks:");
    if (li == std::string::npos) return names;
    std::string tail = s.substr(li + 7);
    for (size_t p = 0; p + 1 < tail.size(); ++p) {               /* bound at next top-level key */
        if (tail[p] == '\n' && tail[p + 1] >= 'a' && tail[p + 1] <= 'z') {
            size_t c = p + 1;
            while (c < tail.size() && ((tail[c] >= 'a' && tail[c] <= 'z') || tail[c] == '_')) ++c;
            if (c < tail.size() && tail[c] == ':') { tail = tail.substr(0, p); break; }
        }
    }
    size_t pos = 0;
    while ((pos = tail.find("name:", pos)) != std::string::npos) {
        size_t ls = tail.rfind('\n', pos);
        bool indented = false;                                    /* only "\n\s+name:" lines (:1212) */
        if (ls != std::string::npos)
            for (size_t q = ls + 1; q < pos; ++q) { if (tail[q] == ' ' || tail[q] == '\t') indented = true; else { indented = false; break; } }
        size_t vs = pos + 5;
        size_t ve = tail.find('\n', vs);
        std::string v = tail.substr(vs, ve == std::string::npos ? std::string::npos : ve - vs);
        size_t a = v.find_first_not_of(" \t\r");
        size_t b = v.find_last_not_of(" \t\r");
        if (indented && a != std::string::npos) names.push_back(v.substr(a, b - a + 1));
        pos = vs;
    }
    return names;
}

MincMenuLists MincMenuListsFor(const std::string &presetKey, const std::string &pinPath) {
    MincMenuLists m;
    m.preset = presetKey;
    m.family = MincFamilyFor(presetKey);
#ifdef AE_OS_WIN
    m.defView = "Windows Video View";
#else
    m.defView = "macOS Video View";                              /* platform video view (:874) */
#endif
    m.defRender = "Video Render";
    MincJsonPtr j = MincJsonParseFile(MincPresetsFileUsed().empty() ? std::string("/dev/null") : MincPresetsFileUsed());
    if (!j) { MincPresetMeta(presetKey); j = MincJsonParseFile(MincPresetsFileUsed()); }  /* force discovery */
    if (!j) return m;
    MincJsonPtr live = j->get("presets");
    MincJsonPtr pr = (live && !presetKey.empty()) ? live->get(presetKey) : nullptr;
    std::vector<std::string> viewOnly;
    JsonList(pr, "inputSpaces", m.input);  if (m.input.empty()) JsonList(j, "inputSpaces", m.input);
    JsonList(pr, "viewSpaces", m.view);    if (m.view.empty()) JsonList(j, "viewSpaces", m.view);
    JsonList(pr, "viewOnly", viewOnly);    if (viewOnly.empty()) JsonList(j, "viewOnly", viewOnly);
    JsonList(pr, "renderSpaces", m.render);
    if (m.render.empty()) JsonList(j, "renderSpaces", m.render);
    if (m.render.empty()) {                                      /* fallback: view+input dedup (:64) */
        std::map<std::string, bool> seen;
        for (auto &v : m.view) if (!seen.count(v)) { seen[v] = 1; m.render.push_back(v); }
        for (auto &v : m.input) if (!seen.count(v)) { seen[v] = 1; m.render.push_back(v); }
    }
    {   /* viewOnly subtraction from render (:61-65) */
        std::map<std::string, bool> vo;
        for (auto &v : viewOnly) vo[v] = true;
        std::vector<std::string> r2;
        for (auto &v : m.render) if (!vo.count(v)) r2.push_back(v);
        m.render.swap(r2);
    }
    /* pinned-config filtering (:66-70): menus follow the PINNED config */
    MincPresetInfo meta = MincPresetMeta(presetKey);
    std::string pinBase = pinPath.substr(pinPath.find_last_of('/') == std::string::npos ? 0 : pinPath.find_last_of('/') + 1);
    if (meta.valid && !meta.retired && !pinBase.empty() && pinBase != meta.config) {
        std::map<std::string, bool> ps = ConfigSpaces(pinPath);
        if (!ps.empty()) {
            auto keep = [&](std::vector<std::string> &l) {
                std::vector<std::string> o;
                for (auto &x : l) if (ps.count(x)) o.push_back(x);
                l.swap(o);
            };
            keep(m.input); keep(m.view); keep(m.render);
        }
    }
    m.looks = ConfigLooks(pinPath);
    m.valid = true;
    return m;
}

MincSuggestCtx MincBuildSuggestCtx(const std::string &presetKey, const std::string &pinPath) {
    MincSuggestCtx ctx;
    ctx.preset = presetKey;
    ctx.family = MincFamilyFor(presetKey);
    ctx.defView = "macOS Video View"; ctx.defRender = "Video Render";
    ctx.video709 = (ctx.family == "Display") ? "working" : "Gamma 2.4 Encoded Rec.709";
    MincMenuLists m = MincMenuListsFor(presetKey, pinPath);
    if (!m.valid) return ctx;
    for (auto &v : m.input) { ctx.validInput[v] = true; ctx.validAll[v] = true; }
    for (auto &v : m.view) ctx.validAll[v] = true;
    for (auto &v : m.render) ctx.validAll[v] = true;
    /* extension table (extDefaults :813) */
    MincJsonPtr ed = MincJsonParseFile(MincSettingsDir() + "/extension-defaults.json");
    if (ed) {
        MincJsonPtr d = ed->get("defaults");
        if (d && d->type == MincJsonValue::Object)
            for (auto &kv : d->obj)
                if (kv.second && kv.second->type == MincJsonValue::String) ctx.extMap[kv.first] = kv.second->strV;
    }
    ctx.valid = true;
    return ctx;
}

/* ---- suggestFromDetected (:833-843) ---- */
static std::string SuggestFromDetected(const std::string &label, bool isStill, const MincSuggestCtx &ctx) {
    if (label.empty()) return "";
    if (label.compare(0, 10, "BT.2100 PQ") == 0) return "Rec.2100-PQ";
    if (label.compare(0, 11, "BT.2100 HLG") == 0) return "Rec.2100-HLG";
    if (label.compare(0, 4, "sRGB") == 0) return "sRGB";
    if (label.compare(0, 10, "Display P3") == 0) return "Display P3";
    if (label.compare(0, 6, "BT.709") == 0 || label.compare(0, 5, "1,1,1") == 0)
        return isStill ? "sRGB" : ctx.video709;
    return "";
}

static std::string Ext(const std::string &fileName) {
    size_t d = fileName.find_last_of('.');
    if (d == std::string::npos || d + 1 >= fileName.size()) return "";
    std::string e = fileName.substr(d + 1);
    for (auto &c : e) c = (char)tolower((unsigned char)c);
    for (auto &c : e) if (!isalnum((unsigned char)c)) return "";
    return e;
}
static std::string AfterSlash2(const std::string &s) {
    size_t k = s.find('/');
    return k == std::string::npos ? s : s.substr(k + 1);
}

MincPick MincSuggestionFor(const MincItemFacts &item,
                           const std::map<int32_t, std::string> &detected,
                           const std::map<int32_t, std::string> &harvestNames,
                           const MincSuggestCtx &ctx) {
    MincPick out;
    if (!item.fileName.empty()) {                            /* extension rule outranks (:907-912) */
        std::string e = Ext(item.fileName);
        auto it = e.empty() ? ctx.extMap.end() : ctx.extMap.find(e);
        if (it != ctx.extMap.end()) {
            const std::string &ed = it->second;
            if (ed == "working") { out.why = "extension rule: identity"; return out; }
            std::string en = NormalizeSpace(ed, ctx.validInput, ctx.family);
            if (en == "working") { out.why = "extension rule: identity"; return out; }
            if (!en.empty()) { out.space = en; out.why = "extension rule"; return out; }
            out.why = "extension rule '" + ed + "' not in config \xe2\x80\x94 skipped"; return out;
        }
    }
    auto h = harvestNames.find(item.id);
    if (h != harvestNames.end() && !h->second.empty()) {     /* harvest (:913-920) */
        std::string raw = AfterSlash2(h->second);
        std::string norm = NormalizeSpace(raw, ctx.validInput, ctx.family);
        if (norm == "working") { out.why = "previously assigned '" + raw + "' is working-native here: identity"; return out; }
        if (!norm.empty()) { out.space = norm; out.why = (norm == raw) ? "previously assigned" : "previously assigned: '" + raw + "' mapped"; return out; }
        out.why = "previously assigned '" + raw + "' has no equivalent in this config \xe2\x80\x94 skipped"; return out;
    }
    auto d = detected.find(item.id);                          /* detected metadata (:921-924) */
    std::string det = NormalizeSpace(SuggestFromDetected(d == detected.end() ? "" : d->second, item.isStill, ctx),
                                     ctx.validInput, ctx.family);
    if (det == "working") { out.why = "detected metadata: identity (video is working-native here)"; return out; }
    if (!det.empty()) { out.space = det; out.why = "detected metadata"; return out; }
    out.why = "no suggestion \xe2\x80\x94 skipped";
    return out;
}

MincRemap MincRemapSpace(const std::string &kind, const std::string &space, const MincSuggestCtx &ctx) {
    MincRemap r;
    if (kind == "look") { r.space = space; return r; }
    if (ctx.validAll.count(space)) { r.space = space; return r; }
    std::string m = LegacyMap(ctx.family, space);
    if (m == "working" && kind == "input") {
        r.identity = true; r.changed = true;
        r.note = "'" + space + "' is working-native in this preset (effect removed)";
        return r;
    }
    if (!m.empty() && ctx.validAll.count(m)) { r.space = m; r.changed = true; r.note = "'" + space + "' \xe2\x86\x92 '" + m + "'"; return r; }
    if (kind == "view" || kind == "render") {
        std::string d = (kind == "view") ? ctx.defView : ctx.defRender;
        r.space = d; r.changed = true;
        r.note = "'" + space + "' not in this preset \xe2\x86\x92 " + kind + " default '" + d + "'";
        return r;
    }
    r.changed = true;
    r.note = "'" + space + "' has no equivalent in this preset";
    return r;
}

MincFxName MincParseFxName(const std::string &nm) {          /* parseFxName (:774-780) */
    MincFxName out;
    const std::string PRE = "minColor: ";
    if (nm.compare(0, PRE.size(), PRE) != 0) return out;
    std::string rest = nm.substr(PRE.size());
    const char *kinds[] = { "view ", "render ", "contain ", "look " };
    const char *names[] = { "view", "render", "contain", "look" };
    for (int i = 0; i < 4; ++i) {
        size_t n = strlen(kinds[i]);
        if (rest.compare(0, n, kinds[i]) == 0) { out.valid = true; out.kind = names[i]; out.space = rest.substr(n); return out; }
    }
    size_t a = rest.find(" \xe2\x86\x92 working");
    if (a == std::string::npos) a = rest.find(" -> working");
    if (a != std::string::npos && a > 0) { out.valid = true; out.kind = "input"; out.space = rest.substr(0, a); }
    return out;
}

bool MincSpaceInPin(const std::string &space, const std::string &pinPath) {
    if (space.empty() || space == "default" || pinPath.empty()) return true;
    std::map<std::string, bool> ps = ConfigSpaces(pinPath);
    if (ps.empty()) return true;                              /* unreadable pin: mirror pinnedSpaces() null */
    return ps.count(space) != 0;
}
