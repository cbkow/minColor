// M3 golden: native "minColor: Utility Layers" — VIEW guide adjustment layer on top +
// RENDER adjustment layer switched off, effects are VARIANTS; second run upserts in place
// (idempotent, actions=updated) using the plugin-menus defaults when args are absent.
(function () { var N = "27-utility-layers"; try {
  var SETTINGS = "/Users/Shared/minColor/settings";
  function writeArgs(json) {
    var f = new File(SETTINGS + "/shell-args.json");
    f.encoding = "UTF-8"; f.lineFeed = "Unix";
    if (f.open("w")) { f.write(json + "\n"); f.close(); }
  }
  function writeAnswers(preset) {
    var f = new File(SETTINGS + "/quiet-answers.json");
    f.encoding = "UTF-8"; f.lineFeed = "Unix";
    if (f.open("w")) { f.write('{ "preset": "' + preset + '" }\n'); f.close(); }
  }
  MincBase.savedProject("util27");
  writeAnswers("acescg");
  app.executeCommand(app.findMenuCommandId("minColor: Set Up Project"));
  var comp = app.project.items.addComp("util", 640, 360, 1, 3, 24);
  comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var id = app.findMenuCommandId("minColor: Utility Layers");
  if (!id) { MincBase.log("NATIVE UTILITY LAYERS UNAVAILABLE"); MincBase.finish(N); return; }
  writeArgs('{ "command": "minColor: Utility Layers", "view": "macOS Video View", "render": "Video Render" }');
  app.executeCommand(id);
  var r1 = MincBase.readJson(SETTINGS + "/reports/utility-layers-last.json");
  if (r1) MincBase.dumpReport("first", r1); else MincBase.log("NATIVE REPORT MISSING");
  MincBase.dumpComp("after-first", comp);
  app.executeCommand(id);                                  // no args: plugin-menus defaults, upsert
  var r2 = MincBase.readJson(SETTINGS + "/reports/utility-layers-last.json");
  if (r2) MincBase.dumpReport("second", r2);
  MincBase.dumpComp("after-second", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
