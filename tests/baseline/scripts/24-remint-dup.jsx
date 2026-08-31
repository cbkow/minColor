// M2 golden: duplicate a layer carrying minColor effects -> shared instance ids ->
// the walk re-mints on pass 1 and the bounded second pass converges (reminted=0).
(function () { var N = "24-remint-dup"; try {
  MincBase.savedProject("remint24");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var comp = app.project.items.addComp("remint", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var fxp = ly.property("ADBE Effect Parade");
  var a = fxp.addProperty("MINC CST");   a.name = "minColor: sRGB → working";
  var b = fxp.addProperty("MINC XFORM"); b.name = "minColor: contain Display P3";
  app.executeCommand(app.findMenuCommandId("minColor: Sync From Names"));   // ids settle
  ly.duplicate();                                                            // dup shares both ids
  var mark = MincBase.logMark();
  app.executeCommand(app.findMenuCommandId("minColor: Sync From Names"));
  var lines = MincBase.syncLinesSince(mark);
  for (var i = 0; i < lines.length && i < 2; i++) MincBase.log("[walk] " + lines[i]);   // the two real passes;
                                                          // idle ticks pumped during executeCommand can append extras
  MincBase.dumpComp("after", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
