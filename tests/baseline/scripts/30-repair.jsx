// M3 golden: native "minColor: Repair" — same-hash re-point via the patch ceremony.
// Break the pin live, Doctor goes yellow with a repairTarget, Repair patches + reopens,
// Doctor is green again. Green-project Repair reports action=none.
(function () { var N = "30-repair"; try {
  var SETTINGS = "/Users/Shared/minColor/settings";
  function writeAnswers(preset) {
    var f = new File(SETTINGS + "/quiet-answers.json");
    f.encoding = "UTF-8"; f.lineFeed = "Unix";
    if (f.open("w")) { f.write('{ "preset": "' + preset + '" }\n'); f.close(); }
  }
  MincBase.savedProject("rep30");
  writeAnswers("acescg");
  app.executeCommand(app.findMenuCommandId("minColor: Set Up Project"));
  var idRepair = app.findMenuCommandId("minColor: Repair");
  var idDoctor = app.findMenuCommandId("minColor: Doctor");
  if (!idRepair || !idDoctor) { MincBase.log("NATIVE REPAIR UNAVAILABLE"); MincBase.finish(N); return; }
  // green project: nothing to repair
  app.executeCommand(idRepair);
  var r0 = MincBase.readJson(SETTINGS + "/reports/repair-last.json");
  if (r0) MincBase.dumpReport("green", r0);
  // break the pin live, then diagnose + repair natively
  // break the pin: copy the pinned config to /tmp, pin it (valid at set time), delete it
  var live = new File(app.project.ocioConfigurationFile);
  var ghost = new File("/tmp/minc-ghost-config.ocio");
  live.copy(ghost.fsName);
  app.project.ocioConfigurationFile = ghost.fsName;
  ghost.remove();                                          // pin now dangles
  app.executeCommand(idDoctor);
  var d = MincBase.readJson(SETTINGS + "/reports/doctor-last.json");
  if (d) MincBase.dumpReport("broken", d);
  app.executeCommand(idRepair);
  var r1 = MincBase.readJson(SETTINGS + "/reports/repair-last.json");
  if (r1) MincBase.dumpReport("repaired", r1); else MincBase.log("NATIVE REPORT MISSING");
  app.executeCommand(idDoctor);
  var d2 = MincBase.readJson(SETTINGS + "/reports/doctor-last.json");
  if (d2) MincBase.dumpReport("after", d2);
  MincBase.dumpProject("state");
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
