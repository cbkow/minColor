// Contract-flip golden: native Strip Foreign OCIO (ours spared, foreign killed) then
// Strip ALL (everything pipeline-shaped goes) on a variant-built comp.
(function () { var N = "08-strip"; try {
  MincBase.savedProject("strip08");
  MincBase.setUpProject("acescg");
  var comp = app.project.items.addComp("strip", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var fx = ly.property("ADBE Effect Parade").addProperty("MINC XFORM");
  fx.name = "minColor: sRGB → working";
  MincBase.runCmd("minColor: Utility Layers",
    '{ "command": "minColor: Utility Layers", "view": "macOS Video View", "render": "Video Render" }',
    null);
  MincBase.runCmd("minColor: Sync From Names", null, null);
  try { ly.property("ADBE Effect Parade").addProperty("ADBE OCIO Display Transform").name = "foreign display"; }
  catch (eD) { MincBase.log("foreign add failed: " + eD.toString()); }
  var r1 = MincBase.runCmd("minColor: Strip Foreign OCIO", null, "strip-foreign");
  if (r1) MincBase.dumpReport("strip-foreign", r1);
  MincBase.dumpComp("after-foreign", comp);
  var r2 = MincBase.runCmd("minColor: Strip ALL", null, "strip-all");
  if (r2) MincBase.dumpReport("strip-all", r2);
  MincBase.dumpComp("after-all", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
