// M3 golden: native "minColor: Adopt Effects" — minColor-named native Adobe effects become
// VARIANTS (verb from the parsed kind). Fixture (contract flip, panel translate retired):
// variants + look built natively, Package for Any AE makes the project native Adobe,
// native Adopt brings it home as variants.
(function () { var N = "29-adopt"; try {
  MincBase.savedProjectNamed("adopt29", "adopt29");
  MincBase.setUpProject("acescg");
  var comp = app.project.items.addComp("adopt", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var fx = ly.property("ADBE Effect Parade").addProperty("MINC XFORM");
  fx.name = "minColor: sRGB → working";
  MincBase.runCmd("minColor: Utility Layers",
    '{ "command": "minColor: Utility Layers", "view": "macOS Video View", "render": "Video Render" }',
    null);
  MincBase.runCmd("minColor: Apply Look",
    '{ "command": "minColor: Apply Look", "look": "Medium Contrast" }', null);
  MincBase.runCmd("minColor: Sync From Names", null, null);
  app.project.save();
  MincBase.runCmd("minColor: Package for Any AE", null, null);   // everything native Adobe (reopens)
  function findComp() {
    for (var i = 1; i <= app.project.numItems; i++)
      if (app.project.item(i) instanceof CompItem && app.project.item(i).name === "adopt") return app.project.item(i);
    return null;
  }
  comp = findComp(); if (comp) comp.openInViewer();
  MincBase.dumpComp("native-dialect", comp);
  var r = MincBase.runCmd("minColor: Adopt Effects", null, "adopt");
  if (r) MincBase.dumpReport("adopt", r);
  MincBase.dumpComp("adopted", findComp());
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
