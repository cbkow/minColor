// Contract-flip golden: the dialect roundtrip, all native — variants (plugin dialect) →
// Package for Any AE (native Adobe dialect; reopens the project) → Adopt Effects (back to
// variants). The 0.9.2 panel's translateEffects two-way is retired with the panel.
(function () { var N = "07-translate-roundtrip"; try {
  MincBase.savedProjectNamed("xlate07", "xlate07");
  MincBase.setUpProject("acescg");
  var comp = app.project.items.addComp("xlate", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var fx = ly.property("ADBE Effect Parade").addProperty("MINC XFORM");
  fx.name = "minColor: sRGB → working";
  MincBase.runCmd("minColor: Utility Layers",
    '{ "command": "minColor: Utility Layers", "view": "macOS Video View", "render": "Video Render" }',
    null);
  MincBase.runCmd("minColor: Sync From Names", null, null);
  MincBase.dumpComp("plugin-dialect", comp);
  app.project.save();
  var p = MincBase.runCmd("minColor: Package for Any AE", null, "package");
  if (p) MincBase.dumpReport("to-native", p);
  function findComp() {                                    // Package reopened the project
    for (var i = 1; i <= app.project.numItems; i++)
      if (app.project.item(i) instanceof CompItem && app.project.item(i).name === "xlate") return app.project.item(i);
    return null;
  }
  var c2 = findComp(); if (c2) { c2.openInViewer(); MincBase.dumpComp("native-dialect", c2); }
  var a = MincBase.runCmd("minColor: Adopt Effects", null, "adopt");
  if (a) MincBase.dumpReport("to-plugin", a);
  var c3 = findComp(); if (c3) MincBase.dumpComp("plugin-again", c3);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
