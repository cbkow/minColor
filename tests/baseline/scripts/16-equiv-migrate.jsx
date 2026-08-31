// M1 equivalence: panel migrateProject("lin709", {}) vs native "minColor: Migrate Project".
(function () { var N = "16-equiv-migrate"; try {
  function writeAnswers(preset) {
    var f = new File("/Users/Shared/minColor/settings/quiet-answers.json");
    f.encoding = "UTF-8"; f.lineFeed = "Unix";
    if (f.open("w")) { f.write('{ "preset": "' + preset + '" }\n'); f.close(); }
  }
  function build(dirTag) {
    MincBase.savedProjectNamed(dirTag, "mig16");
    MincBase.MinColor.applyPresetToCurrent("acescg");
    var png = MincBase.importFixture("ramp_srgb_iccp.png");
    var comp = app.project.items.addComp("mig", 640, 360, 1, 3, 24);
    comp.layers.add(png);
    comp.openInViewer();
    MincBase.MinColor.interpretPass({ mode: "comp" }, null);
    MincBase.MinColor.ensureUtilityLayers(comp, "macOS Video View", "Video Render");
    app.project.save();
  }
  function dumpAll(tag, rep) {
    MincBase.dumpReport(tag, rep);
    MincBase.dumpProject("state");
    for (var i = 1; i <= app.project.numItems; i++)
      if (app.project.item(i) instanceof CompItem && app.project.item(i).name === "mig")
        MincBase.dumpComp("after", app.project.item(i));
  }
  // panel leg
  build("equiv16a");
  var rA = MincBase.MinColor.migrateProject("lin709", {});
  MincBase.captureBegin(); dumpAll("migrate", rA); var a = MincBase.captureEnd();
  // native leg
  build("equiv16b");
  writeAnswers("lin709");
  var id = app.findMenuCommandId("minColor: Migrate Project");
  MincBase.captureBegin();
  if (id) {
    app.executeCommand(id);
    var rB = MincBase.readJson("/Users/Shared/minColor/settings/reports/migrate-last.json");
    if (rB) dumpAll("migrate", rB); else MincBase.log("NATIVE REPORT MISSING");
  } else MincBase.log("NATIVE MIGRATE UNAVAILABLE");
  var b = MincBase.captureEnd();
  MincBase.diffDumps(a, b);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
