(function () { var N = "03-interpret"; try {
  MincBase.savedProject("interp03");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var exr = MincBase.importFixture("ramp_linear.exr");
  var png = MincBase.importFixture("ramp_srgb_iccp.png");
  var mov = MincBase.importFixture("ramp_prores_nclc111.mov");
  var comp = app.project.items.addComp("interp", 640, 360, 1, 3, 24);
  comp.layers.add(exr); comp.layers.add(png); comp.layers.add(mov);
  comp.openInViewer();
  var r = MincBase.MinColor.interpretPass({ mode: "comp" }, null);
  MincBase.dumpReport("interpret", r);
  MincBase.dumpComp("after", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
