// Contract-flip golden: Doctor + Repair on a GREEN project — nothing to repair, doctor
// stays green (30 covers the broken-pin repair leg).
(function () { var N = "09-doctor-repair"; try {
  MincBase.savedProject("repair09");
  MincBase.setUpProject("acescg");
  var d1 = MincBase.runCmd("minColor: Doctor", null, "doctor");
  if (d1) MincBase.dumpReport("doctor-before", d1);
  var r = MincBase.runCmd("minColor: Repair", null, "repair");
  if (r) MincBase.dumpReport("repair", r);
  var d2 = MincBase.runCmd("minColor: Doctor", null, "doctor");
  if (d2) MincBase.dumpReport("doctor-after", d2);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
