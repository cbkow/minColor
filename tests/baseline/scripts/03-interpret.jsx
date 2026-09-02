// Contract-flip golden: native Interpret Timeline — mixed media in the active comp PLUS a
// nested precomp (and a precomp inside that) holding footage: the walk recurses through
// precomp layers, so nested movs are interpreted too (the 2026-09-01 field gap, now covered).
(function () { var N = "03-interpret"; try {
  MincBase.savedProject("interp03");
  MincBase.setUpProject("acescg");
  var exr = MincBase.importFixture("ramp_linear.exr");
  var png = MincBase.importFixture("ramp_srgb_iccp.png");
  var mov = MincBase.importFixture("ramp_prores_nclc111.mov");
  var deep = app.project.items.addComp("deep", 640, 360, 1, 3, 24);    // precomp-in-precomp
  deep.layers.add(mov);
  var inner = app.project.items.addComp("inner", 640, 360, 1, 3, 24);
  inner.layers.add(png);
  inner.layers.add(deep);
  var comp = app.project.items.addComp("interp", 640, 360, 1, 3, 24);
  comp.layers.add(exr); comp.layers.add(inner);
  comp.openInViewer();
  app.project.save();                                      // detected-metadata read needs the saved file
  var r = MincBase.runCmd("minColor: Interpret Timeline", null, "interpret");
  if (r) MincBase.dumpReport("interpret", r);
  MincBase.dumpComp("after", comp);
  MincBase.dumpComp("after-inner", inner);
  MincBase.dumpComp("after-deep", deep);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
