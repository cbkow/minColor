// M3-hardened M2 golden: christening — default-named fresh VIEW/RENDER variants get the
// family default from plugin-menus.json on a christen walk. Phase 1 runs UNMANAGED with the
// menus file deleted: no christening can happen, and the AEGP's gen-change menus writer
// SKIPS non-minColor projects, so the file cannot race back into existence (the original
// scenario deleted it on a managed project and an idle tick could rewrite it — flake of
// record). Phase 2 sets up the project (menus written) and christens via the Sync command.
// Gates proven: no menus -> NO christening; user-typed names untouched; XFORM never
// christened. The walk-pending marker is neutralized so christening stays command-driven.
(function () { var N = "21-christening"; try {
  var SETTINGS = "/Users/Shared/minColor/settings";
  function writeAnswers(preset) {
    var f = new File(SETTINGS + "/quiet-answers.json");
    f.encoding = "UTF-8"; f.lineFeed = "Unix";
    if (f.open("w")) { f.write('{ "preset": "' + preset + '" }\n'); f.close(); }
  }
  function rmSettings(name) { var f = new File(SETTINGS + "/" + name); if (f.exists) f.remove(); }
  function findComp() {
    for (var i = 1; i <= app.project.numItems; i++)
      if (app.project.item(i) instanceof CompItem && app.project.item(i).name === "chr") return app.project.item(i);
    return null;
  }
  MincBase.savedProject("chris21");                         // UNMANAGED: menus writer skips it
  rmSettings("plugin-menus.json");
  var comp = app.project.items.addComp("chr", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var fxp = ly.property("ADBE Effect Parade");
  fxp.addProperty("MINC VIEW");                             // default name "minColor View"
  fxp.addProperty("MINC VIEW");                             // "minColor View 2"
  fxp.addProperty("MINC RENDER");                           // default name
  fxp.addProperty("MINC XFORM");                            // default name — NEVER christened
  var named = fxp.addProperty("MINC VIEW");                 // user-typed name — NEVER overwritten
  named.name = "minColor: view Display P3";
  rmSettings("walk-pending");                               // neutralize the appearance marker
  var mark = MincBase.logMark();
  app.executeCommand(app.findMenuCommandId("minColor: Sync From Names"));
  var l1 = MincBase.syncLinesSince(mark);
  MincBase.log("[no-menus] " + l1[0]);                      // first walk only (idle ticks can append extras)
  MincBase.dumpComp("no-menus", findComp());
  // phase 2: set up the project -> menus exist -> christen via the Sync command
  writeAnswers("acescg");
  app.executeCommand(app.findMenuCommandId("minColor: Migrate Project"));
  rmSettings("walk-pending");
  app.executeCommand(app.findMenuCommandId("minColor: Sync From Names"));
  // no walk-line capture here: idle ticks during the reopen can christen first (timing,
  // not state) — the comp dump below is the deterministic assertion.
  MincBase.dumpComp("christened", findComp());
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
