(function () { var N = "09-doctor-repair"; try {
  MincBase.savedProject("repair09");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  MincBase.dumpReport("doctor-before", MincBase.MinColor.doctor());
  MincBase.dumpReport("repair", MincBase.MinColor.repair());
  MincBase.dumpReport("doctor-after", MincBase.MinColor.doctor());
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
