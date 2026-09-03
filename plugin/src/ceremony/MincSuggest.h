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

/* ORDERED menu lists — the same menuLists (:57-73) algorithm that feeds the ctx maps, kept
   in list form for the plugin-menus.json writer: per-preset arrays with top-level fallback,
   render = dedup(view ∪ input) when absent minus viewOnly, intersected with the pinned
   config's spaces when the pin is behind the preset. Defaults = familyDefaults port (:874-876,
   platform video view + "Video Render"); looks = configLooks port (:1201) from the pin.     */
struct MincMenuLists {
    bool valid = false;
    std::string preset, family, defView, defRender;
    std::vector<std::string> input, view, render, looks;
};
MincMenuLists MincMenuListsFor(const std::string &presetKey, const std::string &pinPath);

/* look NAMES defined in a config (for per-look validity checks). */
std::vector<std::string> MincConfigLooks(const std::string &pinPath);

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
