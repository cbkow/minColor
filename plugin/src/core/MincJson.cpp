#include "MincJson.h"
#include <cstdio>
#include <cstdlib>

MincJsonPtr MincJsonValue::get(const std::string &key) const {
    if (type != Object) return nullptr;
    auto it = obj.find(key);
    return it == obj.end() ? nullptr : it->second;
}
std::string MincJsonValue::str(const std::string &key, const char *dflt) const {
    MincJsonPtr v = get(key);
    return (v && v->type == String) ? v->strV : std::string(dflt);
}
bool MincJsonValue::has(const std::string &key) const { return get(key) != nullptr; }

namespace {
struct P {
    const std::string &s; size_t i = 0; bool ok = true;
    explicit P(const std::string &t) : s(t) {}
    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i; }
    bool lit(const char *l) {
        size_t n = 0; while (l[n]) ++n;
        if (s.compare(i, n, l) != 0) return false;
        i += n; return true;
    }
    void utf8(unsigned c, std::string &out) {
        if (c < 0x80) out += (char)c;
        else if (c < 0x800) { out += (char)(0xC0 | (c >> 6)); out += (char)(0x80 | (c & 0x3F)); }
        else { out += (char)(0xE0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
    }
    bool str(std::string &out) {
        if (i >= s.size() || s[i] != '"') return false;
        ++i;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c == '\\' && i < s.size()) {
                char e = s[i++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': {
                        if (i + 4 > s.size()) return false;
                        unsigned c4 = (unsigned)strtoul(s.substr(i, 4).c_str(), nullptr, 16);
                        i += 4;
                        utf8(c4, out);                                /* BMP only — our files carry no surrogates */
                        break;
                    }
                    default: return false;
                }
            } else out += c;
        }
        return false;
    }
    MincJsonPtr value() {
        ws();
        if (i >= s.size()) { ok = false; return nullptr; }
        auto v = std::make_shared<MincJsonValue>();
        char c = s[i];
        if (c == '{') {
            ++i; v->type = MincJsonValue::Object;
            ws();
            if (i < s.size() && s[i] == '}') { ++i; return v; }
            while (ok) {
                ws();
                std::string k;
                if (!str(k)) { ok = false; break; }
                ws();
                if (i >= s.size() || s[i] != ':') { ok = false; break; }
                ++i;
                MincJsonPtr child = value();
                if (!ok) break;
                v->obj[k] = child;
                ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == '}') { ++i; break; }
                ok = false;
            }
            return ok ? v : nullptr;
        }
        if (c == '[') {
            ++i; v->type = MincJsonValue::Array;
            ws();
            if (i < s.size() && s[i] == ']') { ++i; return v; }
            while (ok) {
                MincJsonPtr child = value();
                if (!ok) break;
                v->arr.push_back(child);
                ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == ']') { ++i; break; }
                ok = false;
            }
            return ok ? v : nullptr;
        }
        if (c == '"') {
            v->type = MincJsonValue::String;
            if (!str(v->strV)) { ok = false; return nullptr; }
            return v;
        }
        if (lit("true"))  { v->type = MincJsonValue::Bool; v->boolV = true;  return v; }
        if (lit("false")) { v->type = MincJsonValue::Bool; v->boolV = false; return v; }
        if (lit("null"))  { v->type = MincJsonValue::Null; return v; }
        {   /* number */
            char *end = nullptr;
            double d = strtod(s.c_str() + i, &end);
            if (end == s.c_str() + i) { ok = false; return nullptr; }
            v->type = MincJsonValue::Number; v->numV = d;
            i = (size_t)(end - s.c_str());
            return v;
        }
    }
};
} // namespace

MincJsonPtr MincJsonParse(const std::string &text) {
    P p(text);
    MincJsonPtr v = p.value();
    if (!p.ok) return nullptr;
    p.ws();
    return v;                                            /* trailing bytes tolerated (files are ours) */
}

MincJsonPtr MincJsonParseFile(const std::string &path) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return nullptr;
    std::string s;
    char buf[16384]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f);
    return MincJsonParse(s);
}
