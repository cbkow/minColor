// Baseline driver — one AE session runs all scenarios in order (app.newProject between them is
// the isolation boundary, same as consecutive panel operations). Launched via DoScript with
// $.global.__mincBase and $.global.__mincOut pre-set by the wrapper the runner generates.
(function () {
  var BASE = $.global.__mincBase, OUT = $.global.__mincOut;
  var marker = new File(OUT + "/DONE.txt");
  // AE's sticky "last project settings" (working space, OCIO pin, bit depth) leak OUT of
  // the suite: seedStickyPref rewrites them and every scenario project updates them, so the
  // USER's next fresh project would inherit minColor's OCIO setup — competing with their
  // Adobe-color-managed defaults (found live 2026-09-02). Capture now, restore at the end.
  var STICKY_SEC = "After Effects Sticky Prefs", STICKY_KEY = "AE Last Project Settings";
  var stickyOrig = null;
  try { stickyOrig = app.preferences.getPrefAsString(STICKY_SEC, STICKY_KEY, PREFType.PREF_Type_MACHINE_INDEPENDENT); } catch (eSP) {}
  function stickyRestore() {
    if (stickyOrig === null) return;
    try {
      app.preferences.savePrefAsString(STICKY_SEC, STICKY_KEY, stickyOrig, PREFType.PREF_Type_MACHINE_INDEPENDENT);
      app.preferences.saveToDisk();
    } catch (eSR) {}
  }
  try {
    app.beginSuppressDialogs();
    $.evalFile(BASE + "/lib/harness.jsxinc");
    var scripts = ["01-doctor-unmanaged", "02-apply-preset", "03-interpret", "04-utility-layers",
                   "05-apply-look", "06-render-preset", "07-translate-roundtrip", "08-strip",
                   "09-doctor-repair", "10-migrate", "11-archive", "12-sync-from-names",
                   // 13-19 (panel-vs-native equivalence) retired at the M3 contract flip
                   "20-variant-sync", "21-christening", "22-plugin-menus", "23-variant-strip-translate", "24-remint-dup",
                   "25-migrate-native-rebuild", "26-interpret-selected", "27-utility-layers", "28-apply-look", "29-adopt",
                   "30-repair", "31-render-preset",
                   "33-legacy-placeholders",                                      // before 32 by design:
                   "32-suggestion-semantics"];   // 32 LAST: it rewrites the pinned ext table
    for (var i = 0; i < scripts.length; i++) {
      try { $.evalFile(BASE + "/scripts/" + scripts[i] + ".jsx"); }
      catch (eS) {   // a scenario that dies before its own catch still gets a result file
        var f = new File(OUT + "/" + scripts[i] + ".txt");
        f.encoding = "UTF-8";
        if (f.open("w")) { f.write("DRIVER-LEVEL ERROR: " + eS.toString() + "\n"); f.close(); }
      }
    }
    try { app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES); } catch (eC) {}
    stickyRestore();                                   /* the user's fresh-project defaults survive */
    app.endSuppressDialogs(false);
    if (marker.open("w")) { marker.write("done\n"); marker.close(); }
  } catch (e) {
    stickyRestore();
    if (marker.open("w")) { marker.write("DRIVER FATAL: " + e.toString() + "\n"); marker.close(); }
  }
})();
