// Contract-flip golden: native Apply Look on freshly-built utility layers (28 covers
// error/replace/remove semantics).
(function () { var N = "05-apply-look"; try {
  MincBase.savedProject("look05");
  MincBase.setUpProject("acescg");
  var comp = app.project.items.addComp("look", 640, 360, 1, 3, 24);
  comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  MincBase.runCmd("minColor: Utility Layers",
    '{ "command": "minColor: Utility Layers", "view": "macOS Video View", "render": "Video Render" }',
    null);
  var r = MincBase.runCmd("minColor: Apply Look",
    '{ "command": "minColor: Apply Look", "look": "Medium Contrast" }', "apply-look");
  if (r) MincBase.dumpReport("look", r);
  MincBase.dumpComp("after", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
