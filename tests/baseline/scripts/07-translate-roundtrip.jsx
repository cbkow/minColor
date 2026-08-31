(function () { var N = "07-translate-roundtrip"; try {
  MincBase.savedProject("xlate07");
  MincBase.MinColor.applyPresetToCurrent("acescg");
  var comp = app.project.items.addComp("xlate", 640, 360, 1, 3, 24);
  var ly = comp.layers.addSolid([0.5, 0.5, 0.5], "subject", 640, 360, 1);
  comp.openInViewer();
  MincBase.MinColor.addInputTransform(ly, "sRGB");
  MincBase.MinColor.ensureUtilityLayers(comp, "macOS Video View", "Video Render");
  MincBase.MinColor.syncPluginNames();
  MincBase.dumpComp("plugin-dialect", comp);
  MincBase.dumpReport("to-native", MincBase.MinColor.translateEffects("native"));
  MincBase.dumpComp("native-dialect", comp);
  MincBase.dumpReport("to-plugin", MincBase.MinColor.translateEffects("plugin"));
  MincBase.dumpComp("plugin-again", comp);
  MincBase.finish(N);
} catch (e) { MincBase.fail(N, e); } })();
