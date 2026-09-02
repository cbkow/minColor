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
  // conform-on-upsert: resize the comp, run again — reused solids must follow the comp
  // (dimensions + PAR + re-centered anchor), not keep their birth size
  comp.width = 512; comp.height = 512; comp.pixelAspect = 1;
  app.executeCommand(app.findMenuCommandId("minColor: Utility Layers"));
  for (var j = 1; j <= comp.numLayers; j++) {
    var s2 = comp.layer(j).source;
    var ap = comp.layer(j).property("ADBE Transform Group").property("ADBE Anchor Point").value;
    MincBase.log("resized layer " + j + " src=" + (s2 ? s2.width + "x" + s2.height + " par=" + s2.pixelAspect : "?") +
                 " anchor=" + ap[0] + "," + ap[1]);
  }
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
