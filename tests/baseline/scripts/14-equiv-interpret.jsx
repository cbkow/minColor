// M1 equivalence: panel interpretPass({mode:"comp"}) vs native "minColor: Interpret Timeline".
(function () { var N = "14-equiv-interpret"; try {
  var REPORT = "/Users/Shared/minColor/settings/reports/interpret-last.json";
  function build(name) {
    MincBase.savedProject(name);
    MincBase.MinColor.applyPresetToCurrent("acescg");
    var exr = MincBase.importFixture("ramp_linear.exr");
    var png = MincBase.importFixture("ramp_srgb_iccp.png");
    var mov = MincBase.importFixture("ramp_prores_nclc111.mov");
    var comp = app.project.items.addComp("interp", 640, 360, 1, 3, 24);
    comp.layers.add(exr); comp.layers.add(png); comp.layers.add(mov);
    app.project.save();                                  // detected-metadata read needs the saved file
    comp.openInViewer();
    return comp;
  }
  // panel side
  var compA = build("equiv14a");
  var rA = MincBase.MinColor.interpretPass({ mode: "comp" }, null);
  MincBase.captureBegin();
  MincBase.dumpReport("interpret", rA);
  MincBase.dumpComp("after", compA);
  var a = MincBase.captureEnd();
  // native side
  var compB = build("equiv14b");
  var id = app.findMenuCommandId("minColor: Interpret Timeline");
  MincBase.captureBegin();
  if (id) {
    app.executeCommand(id);
    var rB = MincBase.readJson(REPORT);
    if (rB) MincBase.dumpReport("interpret", rB); else MincBase.log("NATIVE REPORT MISSING");
    MincBase.dumpComp("after", compB);
  } else MincBase.log("NATIVE INTERPRET UNAVAILABLE");
  var b = MincBase.captureEnd();
  MincBase.diffDumps(a, b);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
