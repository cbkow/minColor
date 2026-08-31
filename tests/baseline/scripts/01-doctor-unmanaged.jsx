(function () { var N = "01-doctor-unmanaged"; try {
  MincBase.MinColor.seedStickyPref("acescg");   // fresh-project working space inherits AE's sticky
  MincBase.freshProject();                       // last-project prefs — seed for determinism
  MincBase.dumpReport("doctor", MincBase.MinColor.doctor());
  MincBase.dumpProject("state");
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
