// M3 golden: native "minColor: Interpret Selected" — active comp's selected layers only,
// explicit space via shell-args bypasses the suggestion engine (why=explicit); the
// unselected layer is untouched; no selection = "select layer(s)" error.
(function () { var N = "26-interpret-selected"; try {
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
  MincBase.savedProject("isel26");
  writeAnswers("acescg");
  app.executeCommand(app.findMenuCommandId("minColor: Set Up Project"));
  var png = MincBase.importFixture("ramp_srgb_iccp.png");
  var mov = MincBase.importFixture("ramp_prores_nclc111.mov");
  var comp = app.project.items.addComp("isel", 640, 360, 1, 3, 24);
  var lyPng = comp.layers.add(png);
  var lyMov = comp.layers.add(mov);
  comp.openInViewer();
  var id = app.findMenuCommandId("minColor: Interpret Selected");
  if (!id) { MincBase.log("NATIVE INTERPRET SELECTED UNAVAILABLE"); MincBase.finish(N); return; }
  // negative: nothing selected
  lyPng.selected = false; lyMov.selected = false;
  app.executeCommand(id);
  var r0 = MincBase.readJson(SETTINGS + "/reports/interpret-selected-last.json");
  if (r0) MincBase.dumpReport("no-selection", r0);
  // positive: PNG selected, explicit space
  lyPng.selected = true; lyMov.selected = false;
  writeArgs('{ "command": "minColor: Interpret Selected", "space": "Display P3" }');
  app.executeCommand(id);
  var r1 = MincBase.readJson(SETTINGS + "/reports/interpret-selected-last.json");
  if (r1) MincBase.dumpReport("explicit", r1); else MincBase.log("NATIVE REPORT MISSING");
  MincBase.dumpComp("after", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
