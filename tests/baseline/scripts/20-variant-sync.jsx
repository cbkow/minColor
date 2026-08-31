// M2 golden: verb authority in the walk. Variants applied by match name, named with the
// grammar; the match name is the verb authority — a contradicting name-verb is unparsed
// (never reinterpreted), bare "minColor: <space>" names parse on variants.
(function () { var N = "20-variant-sync"; try {
  MincBase.savedProject("vsync20");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var comp = app.project.items.addComp("vsync", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  var fxp = ly.property("ADBE Effect Parade");
  function add(match, name) { var fx = fxp.addProperty(match); fx.name = name; }
  add("MINC CST",    "minColor: Rec.709 → working");        // legacy: name carries the verb
  add("MINC XFORM",  "minColor: sRGB → working");           // arrow is XFORM-only
  add("MINC XFORM",  "minColor: contain Display P3");            // contain token lives on XFORM
  add("MINC VIEW",   "minColor: view macOS Video View");         // redundant token, agrees
  add("MINC RENDER", "minColor: Video Render");                  // bare form: space only
  add("MINC LOOK",   "minColor: look Some Look");                // look space stored as-is
  add("MINC VIEW",   "minColor: render Video Render");           // CONTRADICTION -> unparsed
  var mark = MincBase.logMark();
  app.executeCommand(app.findMenuCommandId("minColor: Sync From Names"));
  var lines = MincBase.syncLinesSince(mark);
  MincBase.log("[walk] " + lines[0]);                     // first walk only (idle ticks can append extras)
  MincBase.dumpComp("after-sync", comp);
  MincBase.dumpProject("state");
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
