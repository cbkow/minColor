(function () { var N = "12-sync-from-names"; try {
  MincBase.savedProject("sync12");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var comp = app.project.items.addComp("sync", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var fx = MincBase.MinColor.addInputTransform(ly, "sRGB");
  MincBase.MinColor.syncPluginNames();
  MincBase.dumpComp("synced", comp);
  fx = ly.property("ADBE Effect Parade").property(1);
  fx.name = "minColor: Display P3 → working";
  MincBase.MinColor.syncPluginNames();
  MincBase.dumpComp("renamed-and-synced", comp);
  MincBase.dumpProject("state");
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
