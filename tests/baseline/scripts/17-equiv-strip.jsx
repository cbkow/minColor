// M1 equivalence: panel stripForeignOcio (both modes) vs native strip commands.
(function () { var N = "17-equiv-strip"; try {
  function build(dirTag) {
    MincBase.savedProjectNamed(dirTag, "strip17");
    MincBase.MinColor.applyPresetToCurrent("acescg");
    var comp = app.project.items.addComp("strip", 640, 360, 1, 3, 24);
    var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
    comp.openInViewer();
    MincBase.MinColor.addInputTransform(ly, "sRGB");
    MincBase.MinColor.ensureUtilityLayers(comp, "macOS Video View", "Video Render");
    try { ly.property("ADBE Effect Parade").addProperty("ADBE OCIO Display Transform").name = "foreign display"; }
    catch (eD) { MincBase.log("foreign add failed: " + eD.toString()); }
    return comp;
  }
  // panel leg
  var compA = build("equiv17a");
  MincBase.captureBegin();
  MincBase.dumpReport("strip-foreign", MincBase.MinColor.stripForeignOcio(compA));
  MincBase.dumpComp("after-foreign", compA);
  MincBase.dumpReport("strip-all", MincBase.MinColor.stripForeignOcio(compA, "all"));
  MincBase.dumpComp("after-all", compA);
  var a = MincBase.captureEnd();
  // native leg (commands act on the ACTIVE comp)
  var compB = build("equiv17b");
  var idF = app.findMenuCommandId("minColor: Strip Foreign OCIO");
  var idA = app.findMenuCommandId("minColor: Strip ALL");
  MincBase.captureBegin();
  if (idF && idA) {
    app.executeCommand(idF);
    var rF = MincBase.readJson("/Users/Shared/minColor/settings/reports/strip-foreign-last.json");
    if (rF) MincBase.dumpReport("strip-foreign", rF); else MincBase.log("NATIVE REPORT MISSING");
    MincBase.dumpComp("after-foreign", compB);
    app.executeCommand(idA);
    var rA = MincBase.readJson("/Users/Shared/minColor/settings/reports/strip-all-last.json");
    if (rA) MincBase.dumpReport("strip-all", rA); else MincBase.log("NATIVE REPORT MISSING");
    MincBase.dumpComp("after-all", compB);
  } else MincBase.log("NATIVE STRIP UNAVAILABLE");
  var b = MincBase.captureEnd();
  MincBase.diffDumps(a, b);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
