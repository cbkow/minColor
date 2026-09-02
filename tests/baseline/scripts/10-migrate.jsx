// Contract-flip golden: native Migrate Project — interpreted footage + utility pair on
// acescg, migrated to lin709 (quiet preset answer).
(function () { var N = "10-migrate"; try {
  MincBase.savedProject("mig10");
  MincBase.setUpProject("acescg");
  var png = MincBase.importFixture("ramp_srgb_iccp.png");
  var comp = app.project.items.addComp("mig", 640, 360, 1, 3, 24);
  comp.layers.add(png);
  comp.openInViewer();
  app.project.save();                                      // detected-metadata read needs the saved file
  MincBase.runCmd("minColor: Interpret Timeline", null, null);
  MincBase.runCmd("minColor: Utility Layers",
    '{ "command": "minColor: Utility Layers", "view": "macOS Video View", "render": "Video Render" }',
    null);
  MincBase.writeAnswers("lin709");
  var r = MincBase.runCmd("minColor: Migrate Project", null, "migrate");
  if (r) MincBase.dumpReport("migrate", r);
  MincBase.dumpProject("state");
  for (var i = 1; i <= app.project.numItems; i++)
    if (app.project.item(i) instanceof CompItem && app.project.item(i).name === "mig")
      MincBase.dumpComp("after", app.project.item(i));
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
