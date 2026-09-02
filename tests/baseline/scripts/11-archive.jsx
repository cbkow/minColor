// Contract-flip golden: native Archive Project. The archive record no longer carries a
// "golden" field — the panel's golden render retired with the panel (M2 decision).
(function () { var N = "11-archive"; try {
  MincBase.savedProject("arch11");
  MincBase.setUpProject("acescg");
  var comp = app.project.items.addComp("arch", 640, 360, 1, 3, 24);
  comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  app.project.save();
  var r = MincBase.runCmd("minColor: Archive Project", null, "archive");
  if (r) MincBase.dumpReport("archive", r);
  MincBase.dumpProject("state");
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
