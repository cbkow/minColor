// M1 equivalence: panel applyPresetToCurrent vs native "minColor: Set Up Project".
(function () { var N = "15-equiv-setup"; try {
  function writeAnswers(preset) {
    var f = new File("/Users/Shared/minColor/settings/quiet-answers.json");
    f.encoding = "UTF-8"; f.lineFeed = "Unix";
    if (f.open("w")) { f.write('{ "preset": "' + preset + '" }\n'); f.close(); }
  }
  // panel leg
  MincBase.savedProjectNamed("equiv15a", "setup15");
  var rA = MincBase.MinColor.applyPresetToCurrent("acescg");
  MincBase.captureBegin();
  MincBase.dumpReport("apply", rA);
  MincBase.dumpProject("state");
  var a = MincBase.captureEnd();
  // native leg
  MincBase.savedProjectNamed("equiv15b", "setup15");
  writeAnswers("acescg");
  var id = app.findMenuCommandId("minColor: Set Up Project");
  MincBase.captureBegin();
  if (id) {
    app.executeCommand(id);
    var rB = MincBase.readJson("/Users/Shared/minColor/settings/reports/setup-last.json");
    if (rB) MincBase.dumpReport("apply", rB); else MincBase.log("NATIVE REPORT MISSING");
    MincBase.dumpProject("state");
  } else MincBase.log("NATIVE SETUP UNAVAILABLE");
  var b = MincBase.captureEnd();
  MincBase.diffDumps(a, b);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
