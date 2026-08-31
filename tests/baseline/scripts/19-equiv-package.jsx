// M1 equivalence: panel packageForAnyAE vs native "minColor: Package for Any AE".
// items count normalized: the panel's golden ramp render leaves an orphan solid (native
// golden render deferred to M2), so project item COUNTS differ by design — nothing else may.
(function () { var N = "19-equiv-package"; try {
  function build(dirTag) {
    MincBase.savedProjectNamed(dirTag, "pkg19");
    MincBase.MinColor.applyPresetToCurrent("acescg");
    var png = MincBase.importFixture("ramp_srgb_iccp.png");
    var comp = app.project.items.addComp("pkg", 640, 360, 1, 3, 24);
    comp.layers.add(png);
    comp.openInViewer();
    MincBase.MinColor.interpretPass({ mode: "comp" }, null);
    MincBase.MinColor.ensureUtilityLayers(comp, "macOS Video View", "Video Render");
    app.project.save();
  }
  function norm(lines) {
    var out = [];
    for (var i = 0; i < lines.length; i++) out.push(lines[i].replace(/items=\d+/, "items=#"));
    return out;
  }
  function dumpAll(tag, rep) {
    MincBase.dumpReport(tag, rep);
    MincBase.dumpProject("state");
    for (var i = 1; i <= app.project.numItems; i++)
      if (app.project.item(i) instanceof CompItem && app.project.item(i).name === "pkg")
        MincBase.dumpComp("after", app.project.item(i));
  }
  // panel leg
  build("equiv19a");
  var rA = MincBase.MinColor.packageForAnyAE();
  MincBase.captureBegin(); dumpAll("package", rA); var a = norm(MincBase.captureEnd());
  // native leg (command reopens the project — dumpAll re-finds the comp by name)
  build("equiv19b");
  var id = app.findMenuCommandId("minColor: Package for Any AE");
  MincBase.captureBegin();
  if (id) {
    app.executeCommand(id);
    var rB = MincBase.readJson("/Users/Shared/minColor/settings/reports/package-last.json");
    if (rB) dumpAll("package", rB); else MincBase.log("NATIVE REPORT MISSING");
  } else MincBase.log("NATIVE PACKAGE UNAVAILABLE");
  var b = norm(MincBase.captureEnd());
  MincBase.log("(items count normalized: panel golden render leaves an orphan solid)");
  MincBase.diffDumps(a, b);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
