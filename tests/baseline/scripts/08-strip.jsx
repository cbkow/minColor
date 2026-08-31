(function () { var N = "08-strip"; try {
  MincBase.savedProject("strip08");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var comp = app.project.items.addComp("strip", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  MincBase.MinColor.addInputTransform(ly, "sRGB");
  MincBase.MinColor.ensureUtilityLayers(comp, "macOS Video View", "Video Render");
  try { ly.property("ADBE Effect Parade").addProperty("ADBE OCIO Display Transform").name = "foreign display"; }
  catch (eD) { MincBase.log("foreign add failed: " + eD.toString()); }
  MincBase.dumpReport("strip-foreign", MincBase.MinColor.stripForeignOcio(comp));
  MincBase.dumpComp("after-foreign", comp);
  MincBase.dumpReport("strip-all", MincBase.MinColor.stripForeignOcio(comp, "all"));
  MincBase.dumpComp("after-all", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
