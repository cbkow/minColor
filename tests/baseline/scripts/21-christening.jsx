// M2 golden: christening — a default-named fresh VIEW/RENDER variant gets the family default
// from plugin-menus.json on a christen walk (Sync command christens by intent). Gates proven
// here: no menus file -> NO christening (never invents a space); user-typed names untouched;
// XFORM/LOOK never christened. The walk-pending marker is neutralized so christening stays
// strictly command-driven inside this scenario (the marker path is idle-timed, unscriptable).
(function () { var N = "21-christening"; try {
  function writeAnswers(preset) {
    var f = new File("/Users/Shared/minColor/settings/quiet-answers.json");
    f.encoding = "UTF-8"; f.lineFeed = "Unix";
    if (f.open("w")) { f.write('{ "preset": "' + preset + '" }\n'); f.close(); }
  }
  function rmSettings(name) { var f = new File("/Users/Shared/minColor/settings/" + name); if (f.exists) f.remove(); }
  function findComp() {
    for (var i = 1; i <= app.project.numItems; i++)
      if (app.project.item(i) instanceof CompItem && app.project.item(i).name === "chr") return app.project.item(i);
    return null;
  }
  MincBase.savedProject("chris21");
  writeAnswers("acescg");
  app.executeCommand(app.findMenuCommandId("minColor: Set Up Project"));
  rmSettings("plugin-menus.json");                       // negative case first: menus absent
  var comp = app.project.items.addComp("chr", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var fxp = ly.property("ADBE Effect Parade");
  fxp.addProperty("MINC VIEW");                          // default name "minColor View"
  fxp.addProperty("MINC VIEW");                          // "minColor View 2"
  fxp.addProperty("MINC RENDER");                        // default name
  fxp.addProperty("MINC XFORM");                         // default name — NEVER christened
  var named = fxp.addProperty("MINC VIEW");              // user-typed name — NEVER overwritten
  named.name = "minColor: view Display P3";
  rmSettings("walk-pending");                            // neutralize the appearance marker
  var mark = MincBase.logMark();
  app.executeCommand(app.findMenuCommandId("minColor: Sync From Names"));
  var l1 = MincBase.syncLinesSince(mark);
  MincBase.log("[no-menus] " + l1[0]);                   // first walk only: an idle tick pumped
                                                          // during executeCommand can append an
                                                          // idempotent extra walk (timing, not state)
  MincBase.dumpComp("no-menus", findComp());
  // regenerate menus (Set Up writes the file; reopen invalidates handles)
  app.executeCommand(app.findMenuCommandId("minColor: Set Up Project"));
  rmSettings("walk-pending");
  mark = MincBase.logMark();
  app.executeCommand(app.findMenuCommandId("minColor: Sync From Names"));
  var l2 = MincBase.syncLinesSince(mark);
  MincBase.log("[christen] " + l2[0]);
  MincBase.dumpComp("christened", findComp());
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
