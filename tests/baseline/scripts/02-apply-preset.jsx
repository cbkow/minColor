// Contract-flip golden: native Set Up Project (quiet preset answer) + Doctor after.
(function () { var N = "02-apply-preset"; try {
  MincBase.savedProject("preset02");
  var r = MincBase.setUpProject("acescg");
  if (r) MincBase.dumpReport("apply", r);
  MincBase.dumpProject("state");
  var d = MincBase.runCmd("minColor: Doctor", null, "doctor");
  if (d) MincBase.dumpReport("doctor", d);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
