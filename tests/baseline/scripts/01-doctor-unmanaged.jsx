(function () { var N = "01-doctor-unmanaged"; try {
  MincBase.freshProject();
  MincBase.dumpReport("doctor", MincBase.MinColor.doctor());
  MincBase.dumpProject("state");
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
