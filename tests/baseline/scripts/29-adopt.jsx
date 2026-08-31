// M3 golden: native "minColor: Adopt Effects" — minColor-named native Adobe effects become
// VARIANTS (verb from the parsed kind). Fixture: panel builds plugin dialect (input + utility
// pair + look), panel translate makes it native, native Adopt brings it home as variants.
(function () { var N = "29-adopt"; try {
  var SETTINGS = "/Users/Shared/minColor/settings";
  MincBase.savedProject("adopt29");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var comp = app.project.items.addComp("adopt", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  MincBase.MinColor.addInputTransform(ly, "sRGB");
  MincBase.MinColor.ensureUtilityLayers(comp, "macOS Video View", "Video Render");
  MincBase.MinColor.applyLook("Medium Contrast");
  MincBase.MinColor.translateEffects("native");            // everything native Adobe now
  MincBase.dumpComp("native-dialect", comp);
  var id = app.findMenuCommandId("minColor: Adopt Effects");
  if (!id) { MincBase.log("NATIVE ADOPT UNAVAILABLE"); MincBase.finish(N); return; }
  app.executeCommand(id);
  var r = MincBase.readJson(SETTINGS + "/reports/adopt-last.json");
  if (r) MincBase.dumpReport("adopt", r); else MincBase.log("NATIVE REPORT MISSING");
  MincBase.dumpComp("adopted", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
