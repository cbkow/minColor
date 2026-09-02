/* Native Strip — verbatim port of stripForeignOcio (src/minColor.jsxinc:1285-1339).
   mode "foreign": foreign Display/Look/CST stripped, grades left, minColor kept, contained
   precomp interiors skipped. mode "all": demolition incl. MINC + grades + utility layers.  */
#pragma once
#include <string>
#include "MincCore.h"

std::string MincStripForeignOcio(SPBasicSuite *bp, AEGP_PluginID id, bool all);   /* JSON report */
