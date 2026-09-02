// Contract-flip golden: native Apply Render Preset drives the whole render recipe
// (31 covers unknown-preset + look-removal semantics). Recipes seeded into settings;
// the suite's settings snapshot restores the user's file after the run.
(function () { var N = "06-render-preset"; try {
  var rp = new File(MincBase.SETTINGS + "/render-presets.json");
  rp.encoding = "UTF-8"; rp.lineFeed = "Unix";
  if (rp.open("w")) {
    rp.write('{ "presets": { "Graded Video": { "view": "macOS Video View", "render": "Video Render", "look": "Medium Contrast" } } }\n');
    rp.close();
  }
  MincBase.savedProject("rp06");
  MincBase.setUpProject("acescg");
  var comp = app.project.items.addComp("rp", 640, 360, 1, 3, 24);
  comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var r = MincBase.runCmd("minColor: Apply Render Preset",
    '{ "command": "minColor: Apply Render Preset", "name": "Graded Video" }', "render-preset");
  if (r) MincBase.dumpReport("preset", r);
  MincBase.dumpComp("after", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
