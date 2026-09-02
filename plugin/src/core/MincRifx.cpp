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

/* ---- node-tree parse (AEPPatch parse/replaceBody port :18-44) — needed for NESTED chunks
   (footage ocsp lives under Fold > [Sfdr >] Item > Pin  > CLRS)                          ---- */
struct RNode { size_t start = 0, end = 0; std::string tag, ltype; int parent = -1; int depth = -1; };

static void NodeWalk(const std::string &d, size_t off, size_t end, int depth, int parent, std::vector<RNode> &nodes) {
    while (off + 8 <= end) {
        std::string tag = d.substr(off, 4);
        uint32_t size = 0;
        if (!U32BE(d, off + 4, &size)) return;
        if (tag == "LIST" && off + 12 <= end) {
            RNode n; n.start = off; n.end = off + 8 + size; n.tag = tag; n.ltype = d.substr(off + 8, 4);
            n.parent = parent; n.depth = depth;
            int idx = (int)nodes.size();
            nodes.push_back(n);
            NodeWalk(d, off + 12, off + 8 + size, depth + 1, idx, nodes);
        } else {
            RNode n; n.start = off; n.end = off + 8 + size; n.tag = tag; n.parent = parent; n.depth = depth;
            nodes.push_back(n);
        }
        off += 8 + size + (size & 1);
    }
}
static bool ParseNodes(const std::string &d, std::vector<RNode> &nodes) {
    uint32_t rootSize = 0;
    if (d.size() < 12 || d.compare(0, 4, "RIFX") != 0 || d.compare(8, 4, "Egg!") != 0 || !U32BE(d, 4, &rootSize)) return false;
    RNode root; root.start = 0; root.end = 8 + rootSize; root.tag = "RIFX"; root.ltype = "Egg!"; root.parent = -1; root.depth = -1;
    nodes.push_back(root);
    NodeWalk(d, 12, root.end, 0, 0, nodes);
    return true;
}
static bool ReplaceNodeBody(std::string &d, const std::vector<RNode> &nodes, int idx, const std::string &nb) {
    const RNode &n = nodes[idx];
    size_t oldSize = n.end - n.start - 8;
    uint32_t oldPad = (uint32_t)(oldSize & 1), newPad = (uint32_t)(nb.size() & 1);
    std::string out = d.substr(0, n.start) + n.tag;
    { std::string sz = "0000"; P32BE(sz, 0, (uint32_t)nb.size()); out += sz; }
    out += nb;
    if (newPad) out += '\0';
    out += d.substr(n.end + oldPad);
    long delta = (long)(8 + nb.size() + newPad) - (long)(8 + oldSize + oldPad);
    int p = n.parent;
    while (p >= 0) {                                     /* fix every ancestor's size (incl. RIFX root) */
        size_t ps = nodes[p].start;
        uint32_t v = 0; U32BE(out, ps + 4, &v);
        P32BE(out, ps + 4, (uint32_t)((long)v + delta));
        p = nodes[p].parent;
    }
    d.swap(out);
    return true;
}
static int FindFootageJSONById(const std::string &d, const std::vector<RNode> &nodes, int32_t itemId) {
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].tag != "iide") continue;
        size_t bo = nodes[i].start + 8;
        if (bo + 4 > d.size()) continue;
        const unsigned char *b = (const unsigned char *)d.data() + bo;
        int32_t id = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
        if (id != itemId) continue;
        int itemP = nodes[i].parent;                      /* the Item list */
        for (size_t j = i + 1; j < nodes.size() && nodes[j].start < nodes[itemP].end; ++j) {
            if (nodes[j].tag == "ocsp") {
                for (size_t k = j + 1; k < nodes.size(); ++k)
                    if (nodes[k].tag == "Utf8" && nodes[k].parent == nodes[j].parent) return (int)k;
            }
        }
        return -1;
    }
    return -1;
}

bool MincXmpUpsertElement(std::string &tail, const char *localName, const std::string &content) {
    std::string open = std::string("<minColor:") + localName + ">";
    std::string close = std::string("</minColor:") + localName + ">";
    size_t pkEnd = tail.rfind("<?xpacket end");
    if (pkEnd == std::string::npos) return false;
    size_t a = tail.find(open);
    std::string out;
    long delta;
    if (a != std::string::npos) {                        /* REPLACE inner text (panel semantics :131-136) */
        size_t s = a + open.size();
        size_t b = tail.find(close, s);
        if (b == std::string::npos || b > pkEnd) return false;
        delta = (long)content.size() - (long)(b - s);
        out = tail.substr(0, s) + content + tail.substr(b);
    } else {                                             /* INSERT the whole element before </rdf:RDF> */
        size_t rdfEnd = tail.find("</rdf:RDF>");
        if (rdfEnd == std::string::npos || rdfEnd > pkEnd) return false;
        std::string element = "<rdf:Description rdf:about=\"\" xmlns:minColor=\"https://bialkow.ski/minColor/ns/\">"
                              + open + content + close + "</rdf:Description>";
        delta = (long)element.size();
        out = tail.substr(0, rdfEnd) + element + tail.substr(rdfEnd);
    }
    /* size-neutral rebalance against the packet padding (end="w" contract) */
    size_t pk2 = out.rfind("<?xpacket end");
    if (pk2 == std::string::npos) return false;
    if (delta > 0) {                                     /* consume padding */
        size_t padEnd = pk2, padStart = padEnd;
        while (padStart > 0 && (out[padStart-1] == ' ' || out[padStart-1] == '\n' || out[padStart-1] == '\r' || out[padStart-1] == '\t')) --padStart;
        if ((long)(padEnd - padStart) < delta + 8) return false;   /* padding exhausted — fail BEFORE any write */
        out = out.substr(0, padEnd - delta) + out.substr(padEnd);
    } else if (delta < 0) {                              /* add padding */
        out = out.substr(0, pk2) + std::string((size_t)(-delta), ' ') + out.substr(pk2);
    }
    if (out.size() != tail.size()) return false;
    tail.swap(out);
    return true;
}

bool MincRifxPatchProject(const char *path,
                          const char *configAbs,
                          const char *pwcsBody,
                          const std::vector<MincFootagePatch> &footage,
                          const std::vector<MincXmpUpsert> &xmp,
                          std::string *errOut) {
    auto fail = [&](const char *m) { if (errOut) *errOut = m; return false; };
    FILE *f = fopen(path, "rb");
    if (!f) return fail("cannot open");
    std::string d;
    { char buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof(buf), f)) > 0) d.append(buf, n); }
    fclose(f);
    uint32_t rootSize = 0;
    if (d.size() < 12 || d.compare(0, 4, "RIFX") != 0 || d.compare(8, 4, "Egg!") != 0 || !U32BE(d, 4, &rootSize)) return fail("not an AE RIFX project");
    if (8 + (size_t)rootSize > d.size()) return fail("truncated RIFX");
    std::string tail = d.substr(8 + rootSize);
    std::string rifx = d.substr(0, 8 + rootSize);
    if (configAbs) {
        std::string pinJson = std::string("{\"colorManagementSystem\":1,\"ocioConfigurationFile\":\"") + JsonEsc(configAbs) + "\"}";
        if (!MincRifxReplaceTopUtf8After(rifx, "pcms", pinJson, "pin/cms")) return fail("pcms not found");
    }
    if (pwcsBody) {
        if (!MincRifxReplaceTopUtf8After(rifx, "PwCs", pwcsBody, "working")) return fail("PwCs not found");
    }
    for (auto &fp : footage) {                            /* re-parse after each splice (panel :76) */
        std::vector<RNode> nodes;
        if (!ParseNodes(rifx, nodes)) return fail("parse failed");
        int idx = FindFootageJSONById(rifx, nodes, fp.id);
        if (idx < 0) return fail("footage ocsp JSON not found");
        if (!ReplaceNodeBody(rifx, nodes, idx, fp.profileJSON)) return fail("footage splice failed");
    }
    {   /* trailer invariant: the RIFX rewrite must end exactly where the tail begins */
        std::vector<RNode> check;
        if (!ParseNodes(rifx, check)) return fail("post-patch parse failed");
        if (check[0].end != rifx.size()) return fail("trailer mismatch after patch — aborting, file untouched");
    }
    for (auto &x : xmp)
        if (!MincXmpUpsertElement(tail, x.name.c_str(), x.content)) return fail("xmp upsert failed (padding?)");
    FILE *w = fopen(path, "wb");
    if (!w) return fail("cannot write");
    fwrite(rifx.data(), 1, rifx.size(), w);
    fwrite(tail.data(), 1, tail.size(), w);
    fclose(w);
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
