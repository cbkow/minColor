/* Suggestion machinery — verbatim port of src/minColor.jsxinc :833-925 (+ menuLists :57-73,
   configSpaces :32-47, parseFxName :774-780). Strings must match the panel byte-for-byte:
   the equivalence scenarios diff report labels literally.                                  */
#pragma once
#include <string>
#include <map>
#include <vector>

struct MincSuggestCtx {
    bool valid = false;
    std::string preset, family;
    std::map<std::string, bool> validInput;      /* ctx.valid  (inputSpaces) */
    std::map<std::string, bool> validAll;        /* + view/render */
    std::map<std::string, std::string> extMap;   /* extension -> space ("working" = identity) */
    std::string defView, defRender, video709;    /* familyDefaults */
};

MincSuggestCtx MincBuildSuggestCtx(const std::string &presetKey, const std::string &pinPath);

struct MincPick { std::string space; std::string why; };      /* space=="" means none */
/* item facts the caller collects via AEGP */
struct MincItemFacts {
    int32_t id = 0;
    std::string fileName;                        /* basename of footage file ("" = not file footage) */
    bool isStill = false;
};

MincPick MincSuggestionFor(const MincItemFacts &item,
                           const std::map<int32_t, std::string> &detected,
                           const std::map<int32_t, std::string> &harvestNames,
                           const MincSuggestCtx &ctx);

struct MincRemap { std::string space; bool changed = false; bool identity = false; std::string note; };
MincRemap MincRemapSpace(const std::string &kind, const std::string &space, const MincSuggestCtx &ctx);

struct MincFxName { bool valid = false; std::string kind, space; };  /* parseFxName port */
MincFxName MincParseFxName(const std::string &name);

bool MincSpaceInPin(const std::string &space, const std::string &pinPath);  /* assertSpaceInPin's test (true = ok/unknown) */
