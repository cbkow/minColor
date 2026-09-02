// Contract-flip golden: native Utility Layers (basic create; 27 covers upsert/defaults).
(function () { var N = "04-utility-layers"; try {
  MincBase.savedProject("util04");
  MincBase.setUpProject("acescg");
  var comp = app.project.items.addComp("util", 640, 360, 1, 3, 24);
  comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var r = MincBase.runCmd("minColor: Utility Layers",
    '{ "command": "minColor: Utility Layers", "view": "macOS Video View", "render": "Video Render" }',
    "utility-layers");
  if (r) MincBase.dumpReport("ensure", r);
  MincBase.dumpComp("after", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
