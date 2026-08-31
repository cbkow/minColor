/* Harvested from the v2-probes lab (ProbeV2.cpp, probe G — verified against AE 2026 saves).
   C++ port of AEPPatch.jsxinc's patch mechanics: chunk = tag + u32be size + body + pad byte
   when odd; the colour chunks live at TOP level (direct children of RIFX).                   */
#include "MincRifx.h"
#include <cstdio>
#include <cstdint>

static bool U32BE(const std::string &d, size_t o, uint32_t *out) {
    if (o + 4 > d.size()) return false;
    *out = ((uint32_t)(uint8_t)d[o] << 24) | ((uint32_t)(uint8_t)d[o+1] << 16) |
           ((uint32_t)(uint8_t)d[o+2] << 8) | (uint32_t)(uint8_t)d[o+3];
    return true;
}
static void P32BE(std::string &d, size_t o, uint32_t v) {
    d[o] = (char)(v >> 24); d[o+1] = (char)(v >> 16); d[o+2] = (char)(v >> 8); d[o+3] = (char)v;
}
static std::string B64Enc(const std::string &s) {
    static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < s.size(); i += 3) {
        unsigned a = (uint8_t)s[i];
        unsigned b = (i + 1 < s.size()) ? (uint8_t)s[i+1] : 0;
        unsigned c = (i + 2 < s.size()) ? (uint8_t)s[i+2] : 0;
        unsigned t = (a << 16) | (b << 8) | c;
        out += T[(t >> 18) & 63];
        out += T[(t >> 12) & 63];
        out += (i + 1 < s.size()) ? T[(t >> 6) & 63] : '=';
        out += (i + 2 < s.size()) ? T[t & 63] : '=';
    }
    return out;
}
static std::string JsonEsc(const char *s) {
    std::string o;
    for (const char *p = s; *p; ++p) { if (*p == '"' || *p == '\\') o += '\\'; o += *p; }
    return o;
}

std::string MincRifxProfileJSON(const char *name, const char *family) {
    std::string data = B64Enc(std::string("{\"colorSpace1\":\"") + JsonEsc(name) + "\"}");
    return std::string("{\"baseColorProfile\":{\"colorProfileData\":\"") + data +
           "\",\"colorProfileName\":\"" + JsonEsc(family) + "/" + JsonEsc(name) +
           "\"},\"baseProfileType\":3}";
}

bool MincRifxReplaceTopUtf8After(std::string &d, const char *marker, const std::string &nb, const char *what) {
    uint32_t rootSize = 0;
    if (!U32BE(d, 4, &rootSize)) return false;
    size_t rifxEnd = 8 + rootSize;
    size_t off = 12, hit = 0; uint32_t hitSize = 0; std::string prevTag;
    while (off + 8 <= rifxEnd && off + 8 <= d.size()) {
        std::string tag = d.substr(off, 4);
        uint32_t size = 0;
        if (!U32BE(d, off + 4, &size)) break;
        if (tag == "Utf8" && prevTag == marker) { hit = off; hitSize = size; break; }
        prevTag = tag;
        off += 8 + size + (size & 1);
    }
    if (!hit) { fprintf(stderr, "MincRifx: %s (%s -> Utf8) not found\n", what, marker); return false; }
    uint32_t oldPad = hitSize & 1, newPad = (uint32_t)nb.size() & 1;
    std::string out = d.substr(0, hit) + "Utf8";
    { std::string sz = "0000"; P32BE(sz, 0, (uint32_t)nb.size()); out += sz; }
    out += nb;
    if (newPad) out += '\0';
    out += d.substr(hit + 8 + hitSize + oldPad);
    long delta = (long)(8 + nb.size() + newPad) - (long)(8 + hitSize + oldPad);
    P32BE(out, 4, (uint32_t)((long)rootSize + delta));
    d.swap(out);
    return true;
}

bool MincXmpInsertElement(std::string &tail, const std::string &element) {
    size_t rdfEnd = tail.find("</rdf:RDF>");
    size_t pkEnd = tail.rfind("<?xpacket end");
    if (rdfEnd == std::string::npos || pkEnd == std::string::npos || pkEnd < rdfEnd) return false;
    size_t padEnd = pkEnd, padStart = padEnd;
    while (padStart > rdfEnd && (tail[padStart-1] == ' ' || tail[padStart-1] == '\n' || tail[padStart-1] == '\r' || tail[padStart-1] == '\t')) --padStart;
    if (padEnd - padStart < element.size() + 8) return false;        /* padding too small */
    std::string out = tail.substr(0, rdfEnd) + element + tail.substr(rdfEnd, padEnd - element.size() - rdfEnd) + tail.substr(padEnd);
    if (out.size() != tail.size()) return false;                     /* must stay size-neutral */
    tail.swap(out);
    return true;
}

/* ---- reader: port of AEPPatch.readAssignmentsStream (src/AEPPatch.jsxinc:99-157) ----
   Pruned walk (Fold/Sfdr/Item/Pin /CLRS); ocsp/mcsp apply to the NEXT Utf8 in the SAME list
   (sibling semantics); PwCs at depth 0; iide id is LE u32 while sizes are BE.              */
static std::string ProfName(const std::string &jb) {
    const char *k = "\"colorProfileName\":\"";
    size_t p = jb.find(k);
    if (p == std::string::npos) return std::string();
    p += 20;                                             /* strlen(k) */
    size_t e = jb.find('"', p);
    return e == std::string::npos ? std::string() : jb.substr(p, e - p);
}
static std::string ColorSpace2(const std::string &jb) {
    const char *k = "\"colorSpace2\":\"";
    size_t p = jb.find(k);
    if (p == std::string::npos) return std::string();
    p += 15;
    size_t e = jb.find('"', p);
    return e == std::string::npos ? std::string() : jb.substr(p, e - p);
}

struct RdCtx { int32_t id = 0; bool hasId = false; std::string det; };

static void RdWalk(const std::string &d, size_t off, size_t end, int depth, RdCtx *itemCtx,
                   MincAssignments *out, std::vector<std::pair<RdCtx*, MincAssignItem>> &raw,
                   std::vector<RdCtx*> &ctxPool, bool &pendingWorking) {
    std::string pending;                                 /* "ocsp" | "mcsp" for the NEXT Utf8 sibling */
    while (off + 8 <= end) {
        std::string tag = d.substr(off, 4);
        uint32_t size = 0;
        if (!U32BE(d, off + 4, &size)) return;
        size_t next = off + 8 + size + (size & 1);
        if (tag == "LIST" && off + 12 <= end) {
            std::string lt = d.substr(off + 8, 4);
            if (lt == "Fold" || lt == "Sfdr" || lt == "Item" || lt == "Pin " || lt == "CLRS") {
                RdCtx *ctx = itemCtx;
                if (lt == "Item") { ctx = new RdCtx(); ctxPool.push_back(ctx); }
                RdWalk(d, off + 12, off + 8 + size, depth + 1, ctx, out, raw, ctxPool, pendingWorking);
                if (lt == "Item" && ctx->hasId && !ctx->det.empty() && !out->detected.count(ctx->id))
                    out->detected[ctx->id] = ctx->det;
            }
        } else if (tag == "iide" && itemCtx && size >= 4) {
            const unsigned char *b = (const unsigned char *)d.data() + off + 8;
            itemCtx->id = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
            itemCtx->hasId = true;
        } else if (tag == "ocsp" || tag == "mcsp" || tag == "Mcsp") {
            pending = (tag == "ocsp") ? "ocsp" : "mcsp";
        } else if (tag == "PwCs" && depth == 0) {
            pendingWorking = true;
        } else if (tag == "Utf8" && (!pending.empty() || (pendingWorking && depth == 0))) {
            std::string jb = d.substr(off + 8, size);
            if (pendingWorking && depth == 0) {
                std::string w = ProfName(jb);
                out->working = !w.empty() ? w : (jb == "{}" ? "(none)" : "?");
                pendingWorking = false;
            } else if (pending == "ocsp") {
                std::string cs = ProfName(jb);
                if (!cs.empty()) {
                    MincAssignItem it2; it2.colorspace = cs; it2.view = ColorSpace2(jb);
                    raw.push_back({ itemCtx, it2 });
                }
            } else {
                std::string dl = ProfName(jb);
                if (!dl.empty() && itemCtx && itemCtx->det.empty()) itemCtx->det = dl;
            }
            pending.clear();
        }
        off = next;
    }
}

bool MincRifxReadAssignments(const char *path, MincAssignments *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    std::string d;
    { char buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof(buf), f)) > 0) d.append(buf, n); }
    fclose(f);
    uint32_t rootSize = 0;
    if (d.size() < 12 || d.compare(0, 4, "RIFX") != 0 || d.compare(8, 4, "Egg!") != 0 || !U32BE(d, 4, &rootSize)) return false;
    size_t rifxEnd = 8 + rootSize;
    if (rifxEnd > d.size()) rifxEnd = d.size();
    std::vector<std::pair<RdCtx*, MincAssignItem>> raw;
    std::vector<RdCtx*> pool;
    bool pendingWorking = false;
    RdWalk(d, 12, rifxEnd, 0, nullptr, out, raw, pool, pendingWorking);
    for (auto &r : raw) {                                /* ids resolve after the walk (iide can follow ocsp) */
        MincAssignItem it2 = r.second;
        it2.id = (r.first && r.first->hasId) ? r.first->id : 0;
        out->items.push_back(it2);
    }
    for (auto *c : pool) delete c;
    return true;
}

std::string MincReadFileTail(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return std::string();
    std::string d;
    { char buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof(buf), f)) > 0) d.append(buf, n); }
    fclose(f);
    uint32_t rootSize = 0;
    if (d.size() < 12 || d.compare(0, 4, "RIFX") != 0 || !U32BE(d, 4, &rootSize)) return std::string();
    if (8 + (size_t)rootSize >= d.size()) return std::string();
    return d.substr(8 + rootSize);
}

std::string MincXmpReadElement(const std::string &tail, const char *localName) {
    std::string open = std::string("<minColor:") + localName + ">";
    std::string close = std::string("</minColor:") + localName + ">";
    size_t a = tail.find(open);
    if (a == std::string::npos) return std::string();
    a += open.size();
    size_t b = tail.find(close, a);
    if (b == std::string::npos) return std::string();
    std::string raw = tail.substr(a, b - a), out2;
    for (size_t i = 0; i < raw.size(); ++i) {            /* decode &lt; and &amp; (mirror of :144) */
        if (raw.compare(i, 4, "&lt;") == 0) { out2 += '<'; i += 3; }
        else if (raw.compare(i, 5, "&amp;") == 0) { out2 += '&'; i += 4; }
        else if (raw.compare(i, 6, "&quot;") == 0) { out2 += '"'; i += 5; }
        else out2 += raw[i];
    }
    return out2;
}

bool MincRifxPatchCeremony(const char *path,
                           const char *configAbs,
                           const char *wsName, const char *wsFamily,
                           const char *xmpElement) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    std::string d;
    { char buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof(buf), f)) > 0) d.append(buf, n); }
    fclose(f);
    uint32_t rootSize = 0;
    if (d.size() < 12 || d.compare(0, 4, "RIFX") != 0 || d.compare(8, 4, "Egg!") != 0 || !U32BE(d, 4, &rootSize)) return false;
    if (8 + (size_t)rootSize > d.size()) return false;
    std::string tail = d.substr(8 + rootSize);
    std::string rifx = d.substr(0, 8 + rootSize);
    if (configAbs) {
        std::string pinJson = std::string("{\"colorManagementSystem\":1,\"ocioConfigurationFile\":\"") + JsonEsc(configAbs) + "\"}";
        if (!MincRifxReplaceTopUtf8After(rifx, "pcms", pinJson, "pin/cms")) return false;
    }
    if (wsName && wsFamily) {
        if (!MincRifxReplaceTopUtf8After(rifx, "PwCs", MincRifxProfileJSON(wsName, wsFamily), "working")) return false;
    }
    if (xmpElement) {
        if (!MincXmpInsertElement(tail, xmpElement)) return false;
    }
    FILE *w = fopen(path, "wb");
    if (!w) return false;
    fwrite(rifx.data(), 1, rifx.size(), w);
    fwrite(tail.data(), 1, tail.size(), w);
    fclose(w);
    return true;
}
