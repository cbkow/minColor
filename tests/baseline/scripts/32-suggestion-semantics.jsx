// M3 golden: the 2026-09-01 suggestion-semantics revision (Chris). On a Display/sdr22
// project: (1) detected 709 video maps to Rec.1886 (BT.1886/G2.4), NOT identity — sdr22
// works in G2.2; (2) container extensions (mov/mp4/...) carry the "video709" sentinel and
// fire only as FALLBACK — tagged media keeps winning via detection, untagged video gets
// Rec.1886 instead of "skipped"; (3) migrating 709-video transforms INTO sdr22 remaps them
// to Rec.1886 instead of removing them as identity (LEG_DISPLAY revision).
// Runs LAST in the driver: it overwrites the pinned extension table with a container-rule
// copy (run.sh restores the user's table after the run).
(function () { var N = "32-suggestion-semantics"; try {
  var SETTINGS = "/Users/Shared/minColor/settings";
  function writeAnswers(preset) {
    var f = new File(SETTINGS + "/quiet-answers.json");
    f.encoding = "UTF-8"; f.lineFeed = "Unix";
    if (f.open("w")) { f.write('{ "preset": "' + preset + '" }\n'); f.close(); }
  }
  // container rules on top of the pinned table
  var et = new File(SETTINGS + "/extension-defaults.json");
  et.encoding = "UTF-8"; et.lineFeed = "Unix";
  if (et.open("w")) {
    et.write('{ "defaults": { "png": "sRGB", "exr": "ACEScg", "mov": "video709", "mp4": "video709" } }\n');
    et.close();
  }
  // phase 1: interpret on sdr22 — tagged 709 -> Rec.1886 (detected), untagged -> fallback
  MincBase.savedProject("sem32");
  writeAnswers("sdr22");
  app.executeCommand(app.findMenuCommandId("minColor: Migrate Project"));
  var tagged = MincBase.importFixture("ramp_prores_nclc111.mov");
  var untagged = MincBase.importFixture("ramp_untagged.mov");
  var comp = app.project.items.addComp("sem", 640, 360, 1, 3, 24);
  comp.layers.add(tagged);
  comp.layers.add(untagged);
  comp.openInViewer();
  app.project.save();                                      // detected-metadata read needs the saved file
  app.executeCommand(app.findMenuCommandId("minColor: Interpret Timeline"));
  var r1 = MincBase.readJson(SETTINGS + "/reports/interpret-last.json");
  if (r1) MincBase.dumpReport("sdr22-interpret", r1); else MincBase.log("NATIVE REPORT MISSING");
  MincBase.dumpComp("sdr22-after", comp);
  // phase 2: 709-video transform migrated INTO sdr22 -> remapped to Rec.1886, not removed
  MincBase.savedProjectNamed("sem32b", "sem32mig");
  writeAnswers("acescg");
  app.executeCommand(app.findMenuCommandId("minColor: Migrate Project"));
  var c2 = app.project.items.addComp("mig", 640, 360, 1, 3, 24);
  var ly2 = c2.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  c2.openInViewer();
  var fx2 = ly2.property("ADBE Effect Parade").addProperty("MINC XFORM");
  fx2.name = "minColor: Gamma 2.4 Encoded Rec.709 → working";
  app.executeCommand(app.findMenuCommandId("minColor: Sync From Names"));
  app.project.save();
  writeAnswers("sdr22");
  app.executeCommand(app.findMenuCommandId("minColor: Migrate Project"));
  var r2 = MincBase.readJson(SETTINGS + "/reports/migrate-last.json");
  if (r2) MincBase.dumpReport("migrate", r2); else MincBase.log("NATIVE REPORT MISSING");
  for (var i = 1; i <= app.project.numItems; i++)
    if (app.project.item(i) instanceof CompItem && app.project.item(i).name === "mig")
      MincBase.dumpComp("migrated", app.project.item(i));
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
