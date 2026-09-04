// M3 golden: native "minColor: Apply Render Preset" — recipe {view, render, look} drives
// both utility layers + the look; a recipe without a look REMOVES any existing look
// (pr.look || null semantics); unknown preset errors verbatim. Recipes are seeded into
// settings/render-presets.json by the scenario (the settings snapshot restores after).
(function () { var N = "31-render-preset"; try {
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
  // seed recipes (user-editable settings file; snapshot/restore isolates the suite)
  var rp = new File(SETTINGS + "/render-presets.json");
  rp.encoding = "UTF-8"; rp.lineFeed = "Unix";
  if (rp.open("w")) {
    rp.write('{ "presets": { "Graded Video": { "view": "macOS Video View", "render": "Video Render", "look": "Medium Contrast" }, "Clean Video": { "view": "macOS Video View", "render": "Video Render" } } }\n');
    rp.close();
  }
  MincBase.savedProject("rp31");
  writeAnswers("acescg");
  app.executeCommand(app.findMenuCommandId("minColor: Migrate Project"));
  var comp = app.project.items.addComp("rp", 640, 360, 1, 3, 24);
  comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var id = app.findMenuCommandId("minColor: Apply Render Preset");
  if (!id) { MincBase.log("NATIVE RENDER PRESET UNAVAILABLE"); MincBase.finish(N); return; }
  writeArgs('{ "command": "minColor: Apply Render Preset", "name": "No Such Preset" }');
  app.executeCommand(id);
  var r0 = MincBase.readJson(SETTINGS + "/reports/render-preset-last.json");
  if (r0) MincBase.dumpReport("unknown", r0);
  writeArgs('{ "command": "minColor: Apply Render Preset", "name": "Graded Video" }');
  app.executeCommand(id);
  var r1 = MincBase.readJson(SETTINGS + "/reports/render-preset-last.json");
  if (r1) MincBase.dumpReport("graded", r1); else MincBase.log("NATIVE REPORT MISSING");
  MincBase.dumpComp("after-graded", comp);
  writeArgs('{ "command": "minColor: Apply Render Preset", "name": "Clean Video" }');
  app.executeCommand(id);
  var r2 = MincBase.readJson(SETTINGS + "/reports/render-preset-last.json");
  if (r2) MincBase.dumpReport("clean", r2);
  MincBase.dumpComp("after-clean", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
