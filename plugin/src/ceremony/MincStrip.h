/* Native Strip — verbatim port of stripForeignOcio (src/minColor.jsxinc:1285-1339).
   mode "foreign": foreign Display/Look/CST stripped, grades left, minColor kept, contained
   precomp interiors skipped. mode "all": demolition incl. MINC + grades + utility layers.  */
#pragma once
#include <string>
#include "MincCore.h"

std::string MincStripForeignOcio(SPBasicSuite *bp, AEGP_PluginID id, bool all);   /* JSON report */

/* Pre-reopen migrate safety (2026-09-03): whole-project passes, no active comp needed.
   MincScanNativeOcio counts native OCIO effects (match "ADBE OCIO ...") split into FOREIGN
   (not named "minColor: ") vs minColor-authored. MincStripForeignNativeProject removes only the
   FOREIGN ones across every comp — legacy MINC placeholders and minColor-named natives are left
   for the post-reopen rebuild. Used by Migrate to make a fallback project safe to reopen. */
void MincScanNativeOcio(SPBasicSuite *bp, AEGP_PluginID id, int *foreign, int *mincOwned);
std::string MincStripForeignNativeProject(SPBasicSuite *bp, AEGP_PluginID id);   /* JSON report */
