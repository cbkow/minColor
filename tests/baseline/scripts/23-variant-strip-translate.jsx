// M2 golden: variants through the native ceremonies. Strip Foreign spares all five ours and
// kills foreign natives; Package translates variants verb-from-match (contradictions left as
// plugin effects, looks removed when the preset has none); Strip ALL then dooms everything.
// items count normalized (panel golden render side effects don't apply here, but the Package
// reopen makes counts install-dependent via backups? no — kept literal; sidecar adds no items).
(function () { var N = "23-variant-strip-translate"; try {
  MincBase.savedProjectNamed("var23", "vst23");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var comp = app.project.items.addComp("vst", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var fxp = ly.property("ADBE Effect Parade");
  function add(match, name) { var fx = fxp.addProperty(match); fx.name = name; }
  add("MINC CST",    "minColor: Rec.709 → working");
  add("MINC XFORM",  "minColor: sRGB → working");
  add("MINC VIEW",   "minColor: view macOS Video View");
  add("MINC LOOK",   "minColor: look Some Look");                 // preset has no looks -> removed
  add("MINC VIEW",   "minColor: render Video Render");            // contradiction -> left as plugin
  try { fxp.addProperty("ADBE OCIO Display Transform").name = "foreign display"; }
  catch (eD) { MincBase.log("foreign add failed: " + eD.toString()); }
  app.executeCommand(app.findMenuCommandId("minColor: Sync From Names"));
  app.project.save();
  function findComp() {
    for (var i = 1; i <= app.project.numItems; i++)
      if (app.project.item(i) instanceof CompItem && app.project.item(i).name === "vst") return app.project.item(i);
    return null;
  }
  // phase 1: Strip Foreign — ours (all five) spared, foreign display gone
  app.executeCommand(app.findMenuCommandId("minColor: Strip Foreign OCIO"));
  var r1 = MincBase.readJson("/Users/Shared/minColor/settings/reports/strip-foreign-last.json");
  if (r1) MincBase.dumpReport("strip-foreign", r1); else MincBase.log("NATIVE REPORT MISSING");
  MincBase.dumpComp("after-strip-foreign", findComp());
  // phase 2: Package — variants translate verb-from-match (reopens the project)
  app.project.save();
  app.executeCommand(app.findMenuCommandId("minColor: Package for Any AE"));
  var r2 = MincBase.readJson("/Users/Shared/minColor/settings/reports/package-last.json");
  if (r2) MincBase.dumpReport("package", r2); else MincBase.log("NATIVE REPORT MISSING");
  MincBase.dumpComp("after-package", findComp());
  // phase 3: Strip ALL — everything pipeline-shaped goes, incl. the leftover contradiction
  var c3 = findComp(); if (c3) c3.openInViewer();
  app.executeCommand(app.findMenuCommandId("minColor: Strip ALL"));
  var r3 = MincBase.readJson("/Users/Shared/minColor/settings/reports/strip-all-last.json");
  if (r3) MincBase.dumpReport("strip-all", r3); else MincBase.log("NATIVE REPORT MISSING");
  MincBase.dumpComp("after-strip-all", findComp());
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
