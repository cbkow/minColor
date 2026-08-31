(function () { var N = "11-archive"; try {
  MincBase.savedProject("arch11");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var comp = app.project.items.addComp("arch", 640, 360, 1, 3, 24);
  comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var r = MincBase.MinColor.archiveProject();
  MincBase.dumpReport("archive", r);
  MincBase.dumpProject("state");
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
