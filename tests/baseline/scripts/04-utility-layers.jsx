// Contract-flip golden: native Utility Layers (basic create; 27 covers upsert/defaults).
// The comp is ANAMORPHIC (PAR 2): created solids must inherit the comp's pixel aspect or
// the adjustment pair covers only the square-pixel center (found live 2026-09-02).
(function () { var N = "04-utility-layers"; try {
  MincBase.savedProject("util04");
  MincBase.setUpProject("acescg");
  var comp = app.project.items.addComp("util", 640, 360, 2, 3, 24);
  comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 2);
  comp.openInViewer();
  var r = MincBase.runCmd("minColor: Utility Layers",
    '{ "command": "minColor: Utility Layers", "view": "macOS Video View", "render": "Video Render" }',
    "utility-layers");
  if (r) MincBase.dumpReport("ensure", r);
  MincBase.dumpComp("after", comp);
  for (var i = 1; i <= comp.numLayers; i++) {
    var src = comp.layer(i).source;
    MincBase.log("layer " + i + " src=" + (src ? src.width + "x" + src.height + " par=" + src.pixelAspect : "?"));
  }
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
