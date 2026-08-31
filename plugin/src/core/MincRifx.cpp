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
