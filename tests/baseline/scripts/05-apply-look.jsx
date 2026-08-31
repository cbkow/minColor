(function () { var N = "05-apply-look"; try {
  MincBase.savedProject("look05");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var comp = app.project.items.addComp("look", 640, 360, 1, 3, 24);
  comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  MincBase.MinColor.ensureUtilityLayers(comp, "macOS Video View", "Video Render");
  var looks = MincBase.MinColor.configLooks ? MincBase.MinColor.configLooks() : [];
  MincBase.log("looks available=" + looks.length);
  if (looks.length) {
    var r = MincBase.MinColor.applyLook(looks[0]);
    MincBase.dumpReport("look", r);
  }
  MincBase.dumpComp("after", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
