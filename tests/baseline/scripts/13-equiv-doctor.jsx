// M1 equivalence: panel doctor() vs the native "minColor: Doctor" ceremony, unmanaged + green.
(function () { var N = "13-equiv-doctor"; try {
  var REPORT = "/Users/Shared/minColor/settings/reports/doctor-last.json";
  function nativeDoctor() {
    var id = app.findMenuCommandId("minColor: Doctor");
    if (!id) return null;
    app.executeCommand(id);
    return MincBase.readJson(REPORT);
  }
  function compare(label) {
    MincBase.captureBegin();
    MincBase.dumpReport("doctor", MincBase.MinColor.doctor());
    var a = MincBase.captureEnd();
    var nd = nativeDoctor();
    MincBase.captureBegin();
    if (nd) MincBase.dumpReport("doctor", nd); else MincBase.log("NATIVE DOCTOR UNAVAILABLE");
    var b = MincBase.captureEnd();
    MincBase.log("--- " + label + " ---");
    MincBase.diffDumps(a, b);
  }
  MincBase.freshProject();
  compare("unmanaged (unsaved)");
  MincBase.savedProject("equivdoc13");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  compare("managed green");
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
