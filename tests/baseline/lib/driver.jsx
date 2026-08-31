// Baseline driver — one AE session runs all scenarios in order (app.newProject between them is
// the isolation boundary, same as consecutive panel operations). Launched via DoScript with
// $.global.__mincBase and $.global.__mincOut pre-set by the wrapper the runner generates.
(function () {
  var BASE = $.global.__mincBase, OUT = $.global.__mincOut;
  var marker = new File(OUT + "/DONE.txt");
  try {
    app.beginSuppressDialogs();
    $.evalFile(BASE + "/lib/harness.jsxinc");
    var scripts = ["01-doctor-unmanaged", "02-apply-preset", "03-interpret", "04-utility-layers",
                   "05-apply-look", "06-render-preset", "07-translate-roundtrip", "08-strip",
                   "09-doctor-repair", "10-migrate", "11-archive", "12-sync-from-names",
                   "13-equiv-doctor", "14-equiv-interpret", "15-equiv-setup", "16-equiv-migrate", "17-equiv-strip", "18-equiv-archive", "19-equiv-package",
                   "20-variant-sync", "21-christening", "22-plugin-menus", "23-variant-strip-translate", "24-remint-dup",
                   "25-migrate-native-rebuild", "26-interpret-selected", "27-utility-layers"];
    for (var i = 0; i < scripts.length; i++) {
      try { $.evalFile(BASE + "/scripts/" + scripts[i] + ".jsx"); }
      catch (eS) {   // a scenario that dies before its own catch still gets a result file
        var f = new File(OUT + "/" + scripts[i] + ".txt");
        f.encoding = "UTF-8";
        if (f.open("w")) { f.write("DRIVER-LEVEL ERROR: " + eS.toString() + "\n"); f.close(); }
      }
    }
    try { app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES); } catch (eC) {}
    app.endSuppressDialogs(false);
    if (marker.open("w")) { marker.write("done\n"); marker.close(); }
  } catch (e) {
    if (marker.open("w")) { marker.write("DRIVER FATAL: " + e.toString() + "\n"); marker.close(); }
  }
})();
