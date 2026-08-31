(function () { var N = "10-migrate"; try {
  MincBase.savedProject("mig10");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var png = MincBase.importFixture("ramp_srgb_iccp.png");
  var comp = app.project.items.addComp("mig", 640, 360, 1, 3, 24);
  comp.layers.add(png);
  comp.openInViewer();
  MincBase.MinColor.interpretPass({ mode: "comp" }, null);
  MincBase.MinColor.ensureUtilityLayers(comp, "macOS Video View", "Video Render");
  var r = MincBase.MinColor.migrateProject("lin709", {});
  MincBase.dumpReport("migrate", r);
  MincBase.dumpProject("state");
  for (var i = 1; i <= app.project.numItems; i++)
    if (app.project.item(i) instanceof CompItem && app.project.item(i).name === "mig")
      MincBase.dumpComp("after", app.project.item(i));
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
