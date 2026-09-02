// M2 golden: the AEGP writes plugin-menus.json from presets.json + the pinned config
// (ordered menuLists + familyDefaults + configLooks). Set Up writes it explicitly; the
// generatedBy build stamp is dropped from the dump (changes per commit by design).
(function () { var N = "22-plugin-menus"; try {
  function writeAnswers(preset) {
    var f = new File("/Users/Shared/minColor/settings/quiet-answers.json");
    f.encoding = "UTF-8"; f.lineFeed = "Unix";
    if (f.open("w")) { f.write('{ "preset": "' + preset + '" }\n'); f.close(); }
  }
  MincBase.savedProject("menus22");
  writeAnswers("acescg");
  var id = app.findMenuCommandId("minColor: Set Up Project");
  if (id) app.executeCommand(id); else MincBase.log("NATIVE SETUP UNAVAILABLE");
  var m = MincBase.readJson("/Users/Shared/minColor/settings/plugin-menus.json");
  if (m) { delete m.generatedBy; MincBase.dumpReport("menus", m); }
  else MincBase.log("MENUS FILE MISSING");
  MincBase.dumpProject("state");
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
