// M2 golden: migrate rebuilds minColor-named NATIVE CSTs (M1's strip-and-report deviation
// closed). Build a native-dialect project with the panel (translateEffects), then native
// Migrate: view/render retargeted in place as native CSTs, input rebuilt as MINC (plugin
// dialect, addInputTransform parity).
(function () { var N = "25-migrate-native-rebuild"; try {
  function writeAnswers(preset) {
    var f = new File("/Users/Shared/minColor/settings/quiet-answers.json");
    f.encoding = "UTF-8"; f.lineFeed = "Unix";
    if (f.open("w")) { f.write('{ "preset": "' + preset + '" }\n'); f.close(); }
  }
  MincBase.savedProjectNamed("natreb25", "nat25");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var comp = app.project.items.addComp("nat", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  MincBase.MinColor.addInputTransform(ly, "sRGB");
  MincBase.MinColor.ensureUtilityLayers(comp, "macOS Video View", "Video Render");
  MincBase.MinColor.translateEffects("native");         // -> native minColor-named CSTs
  app.project.save();
  MincBase.dumpComp("before", comp);
  writeAnswers("lin709");
  var id = app.findMenuCommandId("minColor: Migrate Project");
  if (id) {
    app.executeCommand(id);
    var r = MincBase.readJson("/Users/Shared/minColor/settings/reports/migrate-last.json");
    if (r) MincBase.dumpReport("migrate", r); else MincBase.log("NATIVE REPORT MISSING");
    for (var i = 1; i <= app.project.numItems; i++)
      if (app.project.item(i) instanceof CompItem && app.project.item(i).name === "nat")
        MincBase.dumpComp("after", app.project.item(i));
    MincBase.dumpProject("state");
  } else MincBase.log("NATIVE MIGRATE UNAVAILABLE");
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
