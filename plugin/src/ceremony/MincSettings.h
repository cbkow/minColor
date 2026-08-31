/* Ceremony-side settings/report plumbing — minColorAEGP target ONLY (never the effect: the
   effect binary stays behavior-stable through M1). All paths live under the panel's shared
   root so run.sh's settings snapshot/restore self-cleans test artifacts.                    */
#pragma once
#include <string>

std::string MincSettingsDir(void);                    /* /Users/Shared/minColor/settings (created) */
bool        MincQuietMode(void);                      /* settings/quiet-mode marker present? */
bool        MincWriteTextFile(const std::string &path, const std::string &content);
std::string MincReadTextFile(const std::string &path);/* "" when absent */
bool        MincWriteReport(const char *ceremony, const std::string &json);  /* settings/reports/<c>-last.json */

/* settings/aegp-api.json — the M3 shell's hard gate. Rewritten whenever the command set
   changes so the commands[] list is always truthful. */
bool MincWriteHandshake(const char *const *commandLabels, int n);
