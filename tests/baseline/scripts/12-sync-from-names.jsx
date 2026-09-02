// Contract-flip golden: names are truth — a variant added by hand syncs from its name,
// a user RENAME re-syncs to the new space (the panel's syncPluginNames, now the walk).
(function () { var N = "12-sync-from-names"; try {
  MincBase.savedProject("sync12");
  MincBase.setUpProject("acescg");
  var comp = app.project.items.addComp("sync", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var fx = ly.property("ADBE Effect Parade").addProperty("MINC XFORM");
  fx.name = "minColor: sRGB → working";
  MincBase.runCmd("minColor: Sync From Names", null, null);
  MincBase.dumpComp("synced", comp);
  fx = ly.property("ADBE Effect Parade").property(1);
  fx.name = "minColor: Display P3 → working";
  MincBase.runCmd("minColor: Sync From Names", null, null);
  MincBase.dumpComp("renamed-and-synced", comp);
  MincBase.dumpProject("state");
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
