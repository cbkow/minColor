/* Minimal nested JSON reader — core (portable, no deps, both binaries). Read-only, tolerant:
   parses objects/arrays/strings (incl. \uXXXX -> UTF-8)/numbers/bool/null into a tree. The
   consumers are OUR OWN generated files (presets.json, extension-defaults.json, minColor.json)
   — unknown constructs fail the parse, never crash.                                          */
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>

struct MincJsonValue;
using MincJsonPtr = std::shared_ptr<MincJsonValue>;

struct MincJsonValue {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool                                boolV = false;
    double                              numV = 0;
    std::string                         strV;
    std::vector<MincJsonPtr>            arr;
    std::map<std::string, MincJsonPtr>  obj;    /* insertion order not preserved; fine for lookups */

    /* lookup helpers — safe on wrong types (return null/defaults) */
    MincJsonPtr get(const std::string &key) const;                 /* object member or nullptr */
    std::string str(const std::string &key, const char *dflt = "") const;
    bool        has(const std::string &key) const;
};

MincJsonPtr MincJsonParse(const std::string &text);                /* nullptr on parse failure */
MincJsonPtr MincJsonParseFile(const std::string &path);            /* nullptr on missing/failure */
