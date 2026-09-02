// M2 golden: migrate rebuilds minColor-named NATIVE CSTs (M1's strip-and-report deviation
// closed). Build the native dialect via Package for Any AE (contract flip: the panel's
// translateEffects retired), then native Migrate: view/render retargeted in place as native
// CSTs, input rebuilt as MINC (plugin dialect).
(function () { var N = "25-migrate-native-rebuild"; try {
  MincBase.savedProjectNamed("natreb25", "nat25");
  MincBase.setUpProject("acescg");
  var comp = app.project.items.addComp("nat", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var fx = ly.property("ADBE Effect Parade").addProperty("MINC XFORM");
  fx.name = "minColor: sRGB → working";
  MincBase.runCmd("minColor: Utility Layers",
    '{ "command": "minColor: Utility Layers", "view": "macOS Video View", "render": "Video Render" }',
    null);
  MincBase.runCmd("minColor: Sync From Names", null, null);
  app.project.save();
  MincBase.runCmd("minColor: Package for Any AE", null, null);   // -> native minColor-named CSTs (reopens)
  function findComp() {
    for (var i = 1; i <= app.project.numItems; i++)
      if (app.project.item(i) instanceof CompItem && app.project.item(i).name === "nat") return app.project.item(i);
    return null;
  }
  comp = findComp(); if (comp) comp.openInViewer();
  MincBase.dumpComp("before", comp);
  MincBase.writeAnswers("lin709");
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
