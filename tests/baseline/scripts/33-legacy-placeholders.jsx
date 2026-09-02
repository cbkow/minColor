// M3 step 8 golden: legacy placeholders. Mac no longer registers "MINC CST" — a 1.x-era
// project opens as benign placeholders (RESULTS §35): the walk counts them without writing,
// Migrate RESURRECTS them as the variants that own their kinds (same grammar names, same
// slots), and Strip Foreign dooms them as dead weight. Fixture: legacy-minc.aep (baked,
// permanent — comp 'legacy', input ×2 / view / render / contain / look, acescg).
(function () { var N = "33-legacy-placeholders"; try {
  var SETTINGS = "/Users/Shared/minColor/settings";
  function firstComp() {
    for (var i = 1; i <= app.project.numItems; i++)
      if (app.project.item(i) instanceof CompItem) return app.project.item(i);
    return null;
  }
  // phase 1: walk counters on open placeholders, then migrate -> resurrection
  MincBase.openFixtureCopy("legacy-minc.aep", "leg33a");
  var mark = MincBase.logMark();
  app.executeCommand(app.findMenuCommandId("minColor: Sync From Names"));
  var lines = MincBase.syncLinesSince(mark);
  MincBase.log("[walk] " + lines[0]);                     // first walk only (idle ticks can append extras)
  MincBase.writeAnswers("acescg");
  app.executeCommand(app.findMenuCommandId("minColor: Migrate Project"));
  var r = MincBase.readJson(SETTINGS + "/reports/migrate-last.json");
  if (r) MincBase.dumpReport("migrate", r); else MincBase.log("NATIVE REPORT MISSING");
  var c1 = firstComp();
  if (c1) MincBase.dumpComp("resurrected", c1);
  // phase 2: fresh copy — strip-foreign dooms placeholders in foreign mode too
  MincBase.openFixtureCopy("legacy-minc.aep", "leg33b");
  var c2 = firstComp();
  if (c2) c2.openInViewer();
  app.executeCommand(app.findMenuCommandId("minColor: Strip Foreign OCIO"));
  var r2 = MincBase.readJson(SETTINGS + "/reports/strip-foreign-last.json");
  if (r2) MincBase.dumpReport("strip-foreign", r2); else MincBase.log("NATIVE REPORT MISSING");
  if (c2) MincBase.dumpComp("after-strip", c2);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
