(function () { var N = "04-utility-layers"; try {
  MincBase.savedProject("util04");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var comp = app.project.items.addComp("util", 640, 360, 1, 3, 24);
  comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var r = MincBase.MinColor.ensureUtilityLayers(comp, "macOS Video View", "Video Render");
  MincBase.dumpReport("ensure", r);
  MincBase.dumpComp("after", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
