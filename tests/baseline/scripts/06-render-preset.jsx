(function () { var N = "06-render-preset"; try {
  MincBase.savedProject("rp06");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var comp = app.project.items.addComp("rp", 640, 360, 1, 3, 24);
  comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  MincBase.MinColor.ensureUtilityLayers(comp, "macOS Video View", "Video Render");
  var rp = MincBase.MinColor.renderPresets ? MincBase.MinColor.renderPresets() : {};
  var first = null, k; for (k in rp) { first = k; break; }
  MincBase.log("render preset used=" + (first || "(none)"));
  if (first) MincBase.dumpReport("preset", MincBase.MinColor.applyRenderPreset(first));
  MincBase.dumpComp("after", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
