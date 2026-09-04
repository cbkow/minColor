// M3 golden: native "minColor: Apply Look" — set/replace/remove the MINC LOOK partner on
// the EXISTING utility layers (spaces + enabled states untouched); look sits at index 1
// (look-then-display); error when no utility layers exist.
(function () { var N = "28-apply-look"; try {
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
  MincBase.savedProject("look28");
  writeAnswers("acescg");
  app.executeCommand(app.findMenuCommandId("minColor: Migrate Project"));
  var comp = app.project.items.addComp("look", 640, 360, 1, 3, 24);
  comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var idLook = app.findMenuCommandId("minColor: Apply Look");
  var idUtil = app.findMenuCommandId("minColor: Utility Layers");
  if (!idLook || !idUtil) { MincBase.log("NATIVE COMMANDS UNAVAILABLE"); MincBase.finish(N); return; }
  // error case: no utility layers yet
  writeArgs('{ "command": "minColor: Apply Look", "look": "Medium Contrast" }');
  app.executeCommand(idLook);
  var r0 = MincBase.readJson(SETTINGS + "/reports/apply-look-last.json");
  if (r0) MincBase.dumpReport("no-layers", r0);
  // build the pair, then set / replace / remove
  writeArgs('{ "command": "minColor: Utility Layers", "view": "macOS Video View", "render": "Video Render" }');
  app.executeCommand(idUtil);
  writeArgs('{ "command": "minColor: Apply Look", "look": "Medium Contrast" }');
  app.executeCommand(idLook);
  var r1 = MincBase.readJson(SETTINGS + "/reports/apply-look-last.json");
  if (r1) MincBase.dumpReport("set", r1);
  MincBase.dumpComp("after-set", comp);
  writeArgs('{ "command": "minColor: Apply Look", "look": "High Contrast" }');
  app.executeCommand(idLook);
  var r2 = MincBase.readJson(SETTINGS + "/reports/apply-look-last.json");
  if (r2) MincBase.dumpReport("replace", r2);
  MincBase.dumpComp("after-replace", comp);
  writeArgs('{ "command": "minColor: Apply Look", "look": "" }');
  app.executeCommand(idLook);
  var r3 = MincBase.readJson(SETTINGS + "/reports/apply-look-last.json");
  if (r3) MincBase.dumpReport("remove", r3);
  MincBase.dumpComp("after-remove", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
