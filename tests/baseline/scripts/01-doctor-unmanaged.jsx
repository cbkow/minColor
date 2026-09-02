// Contract-flip golden: native Doctor on a fresh unmanaged project.
(function () { var N = "01-doctor-unmanaged"; try {
  MincBase.seedStickyPref("acescg");             // fresh-project working space inherits AE's sticky
  MincBase.freshProject();                       // last-project prefs — seed for determinism
  var r = MincBase.runCmd("minColor: Doctor", null, "doctor");
  if (r) MincBase.dumpReport("doctor", r);
  MincBase.dumpProject("state");
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
