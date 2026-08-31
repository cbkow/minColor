(function () { var N = "02-apply-preset"; try {
  MincBase.savedProject("preset02");
  var r = MincBase.MinColor.applyPresetToCurrent("acescg");
  MincBase.dumpReport("apply", r);
  MincBase.dumpProject("state");
  MincBase.dumpReport("doctor", MincBase.MinColor.doctor());
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
