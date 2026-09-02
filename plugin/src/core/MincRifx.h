/* RIFX .aep byte surgery — the native half of the ceremonies (probe G, RESULTS.md §30e).
   AE-SDK-free. The ceremony: AEGP_SaveProjectToPath (saves a COPY) -> patch ->
   AEGP_OpenProjectFromPath. Patchable today: `pcms`->Utf8 (colorManagementSystem + OCIO pin —
   cms:1 turns OCIO mode ON; the chunk exists on every fresh AE 2026 save), `PwCs`->Utf8
   (working space), and the XMP trailer (end="w" packet: elements inserted padding-neutral). */
#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>

std::string MincRifxProfileJSON(const char *name, const char *family);   /* PwCs body for a working space */

/* ---- reader (AEPPatch readAssignments port, RESULTS §9-10 format facts) ----
   working = project working space profile name ("Family/Name", "(none)", or "?");
   items   = footage-level ocsp assignments; detected = AE's mcsp metadata label per item id.
   Item ids are LITTLE-endian in `iide`; chunk sizes are big-endian.                        */
struct MincAssignItem { int32_t id = 0; std::string colorspace; std::string view; };
struct MincAssignments {
    std::string working;
    std::vector<MincAssignItem> items;
    std::map<int32_t, std::string> detected;
};
bool MincRifxReadAssignments(const char *path, MincAssignments *out);

/* XMP trailer element read (mirror of the panel's readProvenance/readEngine :141-159):
   returns the decoded (&lt; &amp;) text of <minColor:localName>…</…>, "" when absent. */
std::string MincXmpReadElement(const std::string &xmpTail, const char *localName);
std::string MincReadFileTail(const char *path);          /* the XMP trailer of a saved .aep ("" on failure) */

/* in-memory RIFX ops on the blob (RIFX portion only, no trailer): */
bool MincRifxReplaceTopUtf8After(std::string &rifx, const char *marker, const std::string &newBody, const char *what);

/* XMP trailer op (size-neutral: consumes packet padding): */
bool MincXmpInsertElement(std::string &xmpTail, const std::string &element);

/* the whole ceremony patch on a saved .aep: cms+pin, working space, provenance element.
   Any nullptr argument skips that part. Verifies structure; on any failure the file is
   left untouched. */
bool MincRifxPatchCeremony(const char *path,
                           const char *configAbsOrNull,
                           const char *wsNameOrNull, const char *wsFamilyOrNull,
                           const char *xmpElementOrNull);

/* XMP UPSERT — replaces an existing <minColor:name> element's text in place, else inserts
   before </rdf:RDF>. Size-neutral in BOTH directions (growth consumes packet padding,
   shrink adds padding) — re-migrating a project must never duplicate elements.           */
bool MincXmpUpsertElement(std::string &xmpTail, const char *localName, const std::string &content);

/* Migrate/Set-Up project patch — pin+cms, raw PwCs body (presets.json pwcsJSON verbatim),
   footage-level strips (ocsp Utf8 by LITTLE-endian item id, node-tree parse with parent
   size fix-ups), and XMP upserts. All-or-nothing: any failure leaves the file untouched. */
struct MincFootagePatch { int32_t id = 0; std::string profileJSON; };
struct MincXmpUpsert { std::string name, content; };
bool MincRifxPatchProject(const char *path,
                          const char *configAbsOrNull,
                          const char *pwcsBodyOrNull,
                          const std::vector<MincFootagePatch> &footage,
                          const std::vector<MincXmpUpsert> &xmp,
                          std::string *errOut);
