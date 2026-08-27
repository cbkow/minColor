// minColor Panel.jsx — v0.4: Doctor status line, Set Up / Migrate dialog, live interpret tools.
// Single-authority model (PREPLAN 3.6b): footage level is always NOTHING; interpretation = layer CSTs.
// Truth lives in the project sidecar (<project>/_minColor); the Doctor repairs dead paths in a click.

#include "AEPPatch.jsxinc"
#include "minColor.jsxinc"

(function (thisObj) {
 try {
  var win;
  if (thisObj instanceof Panel) win = thisObj;
  else {
    try { if ($.global.__minColorWin) $.global.__minColorWin.close(); } catch (e) {}
    win = new Window("palette", "minColor", undefined, { resizeable: true });
    $.global.__minColorWin = win;                     // keep the palette alive after the launching script returns
  }
  win.orientation = "column"; win.alignChildren = ["fill", "top"]; win.spacing = 6; win.margins = 10;
  var presets = {}, keys = [], lists = { inputSpaces: [], viewSpaces: [] }, payloadError = null;
  try {
    presets = MinColor.presets(); for (var k in presets) keys.push(k);
    lists = MinColor.menuLists();
  } catch (ePayload) { payloadError = ePayload.toString(); }   // fail soft: panel opens, Doctor line explains

  function log(s) { status.text = String(s).substr(0, 240); }
  function unstick() {                                              // ScriptUI leaves the clicked button looking pressed until focus moves — force a repaint
    try { win.update(); } catch (e3) {}
  }
  function guard(label, fn) {
    try { log(label + "\u2026"); unstick(); } catch (eB) {}
    try { var r = fn(); log(label + ": " + (r === undefined ? "ok" : r)); refreshDoctor(); }
    catch (e) { log(label + " FAILED: " + e.toString()); alert("minColor — " + label + " failed:\n" + e.toString()); unstick(); }
  }

  // ---- remembered choices: dropdowns restore their last-used value and persist on change ----
  var UIS = {}; try { UIS = MinColor.uiState() || {}; } catch (eU) {}
  function bindDD(dd, key) {
    if (UIS[key]) for (var bi = 0; bi < dd.items.length; bi++) if (dd.items[bi].text === UIS[key]) { dd.selection = bi; break; }
    if (dd.selection) dd.helpTip = dd.selection.text;
    dd.onChange = function () { if (dd.selection) { dd.helpTip = dd.selection.text; UIS[key] = dd.selection.text; try { MinColor.saveUiState(UIS); } catch (eS) {} } };
  }

  // ---- flat-icon toolkit: owner-drawn vector glyphs (no image assets, theme-neutral) ----
  var IC = [0.72, 0.72, 0.72, 1];
  function pen(g, w) { return g.newPen(g.PenType.SOLID_COLOR, IC, w || 1.4); }
  function brush(g) { return g.newBrush(g.BrushType.SOLID_COLOR, IC); }
  var GLYPH = {
    gear: function (g) {
      g.newPath(); g.ellipsePath(5, 5, 8, 8); g.strokePath(pen(g, 1.6));
      for (var i = 0; i < 8; i++) { var a = i * Math.PI / 4, c = Math.cos(a), s = Math.sin(a);
        g.newPath(); g.moveTo(9 + c * 4.6, 9 + s * 4.6); g.lineTo(9 + c * 7.2, 9 + s * 7.2); g.strokePath(pen(g, 1.6)); }
      g.newPath(); g.ellipsePath(7.8, 7.8, 2.4, 2.4); g.fillPath(brush(g));
    },
    film: function (g) {
      g.newPath(); g.rectPath(3, 4.5, 12, 9); g.strokePath(pen(g, 1.3));
      for (var i = 0; i < 3; i++) { g.newPath(); g.rectPath(4.6 + i * 3.4, 5.6, 1.6, 1.6); g.fillPath(brush(g));
        g.newPath(); g.rectPath(4.6 + i * 3.4, 10.8, 1.6, 1.6); g.fillPath(brush(g)); }
    },
    bars: function (g) {
      g.newPath(); g.rectPath(3, 4.4, 10, 2.4); g.fillPath(brush(g));
      g.newPath(); g.rectPath(6, 7.8, 9, 2.4); g.fillPath(brush(g));
      g.newPath(); g.rectPath(4, 11.2, 7, 2.4); g.fillPath(brush(g));
    },
    adj: function (g) {
      g.newPath(); g.ellipsePath(3.5, 3.5, 11, 11); g.strokePath(pen(g, 1.4));
      g.newPath(); g.moveTo(9, 3.5);
      for (var a = 90; a <= 270; a += 15) g.lineTo(9 + 5.5 * Math.cos(a * Math.PI / 180), 9 - 5.5 * Math.sin(a * Math.PI / 180));
      g.fillPath(brush(g));
    },
    eye: function (g) {
      g.newPath(); g.ellipsePath(2.5, 5.5, 13, 7); g.strokePath(pen(g, 1.4));
      g.newPath(); g.ellipsePath(7.4, 7.4, 3.2, 3.2); g.fillPath(brush(g));
    },
    clap: function (g) {
      g.newPath(); g.rectPath(3, 8, 12, 6.5); g.strokePath(pen(g, 1.3));
      g.newPath(); g.rectPath(3, 4, 12, 3.2); g.strokePath(pen(g, 1.3));
      g.newPath(); g.moveTo(6, 4); g.lineTo(7.6, 7.2); g.strokePath(pen(g, 1.1));
      g.newPath(); g.moveTo(10, 4); g.lineTo(11.6, 7.2); g.strokePath(pen(g, 1.1));
    }
  };
  function iconize(btn, glyph, tip) {
    if (tip) btn.helpTip = tip;
    btn.hov = false;
    btn.onDraw = function () {
      var g = this.graphics, s = this.size;
      if (this.hov) { g.newPath(); g.rectPath(0, 0, s[0], s[1]); g.fillPath(g.newBrush(g.BrushType.SOLID_COLOR, [1, 1, 1, 0.09])); }
      glyph(g);
    };
    btn.addEventListener("mouseover", function () { this.hov = true; try { win.update(); } catch (e) {} });
    btn.addEventListener("mouseout", function () { this.hov = false; try { win.update(); } catch (e) {} });
    return btn;
  }
  function section(title, glyph) {                                  // slim drawn header (icon + bold label + hairline) over an indented body group
    var hdr = win.add("group"); hdr.spacing = 6; hdr.alignChildren = ["left", "center"]; hdr.alignment = ["fill", "top"]; hdr.margins = [0, 6, 0, 0];
    var ic = hdr.add("iconbutton", undefined, undefined, { style: "toolbutton" }); ic.preferredSize = [18, 18];
    ic.onDraw = function () { glyph(this.graphics); };
    var st = hdr.add("statictext", undefined, title);
    try { st.graphics.font = ScriptUI.newFont("dialog", "BOLD", 11); } catch (eH) {}
    var ln = hdr.add("panel"); ln.alignment = ["fill", "center"]; ln.preferredSize.height = 2; ln.minimumSize.width = 20;
    var body = win.add("group"); body.orientation = "column"; body.alignChildren = ["fill", "top"]; body.spacing = 4; body.margins = [22, 0, 0, 2];
    return body;
  }

  // ---- Doctor status line: owner-drawn live lamp (click = re-check; a 5 s heartbeat keeps it live) ----
  var rowDoc = win.add("group"); rowDoc.alignChildren = ["left", "center"];
  var dot = rowDoc.add("iconbutton", undefined, undefined, { style: "toolbutton" });
  dot.preferredSize = [18, 18]; dot.dotColor = [0.6, 0.6, 0.6, 1];
  dot.helpTip = "minColor Doctor \u2014 click to re-check now (auto-checks every 5 s)";
  dot.onDraw = function () {
    var g = this.graphics;
    g.newPath(); g.ellipsePath(3, 3, 12, 12);
    g.fillPath(g.newBrush(g.BrushType.SOLID_COLOR, this.dotColor));
    g.strokePath(g.newPen(g.PenType.SOLID_COLOR, [0, 0, 0, 0.45], 1));
    g.newPath(); g.ellipsePath(5.5, 5, 5, 3.4);                    // specular glint
    g.fillPath(g.newBrush(g.BrushType.SOLID_COLOR, [1, 1, 1, 0.28]));
  };
  var docText = rowDoc.add("statictext", undefined, "…", { truncate: "end" }); docText.alignment = ["fill", "center"];
  var bRepair = rowDoc.add("button", undefined, "Repair"); bRepair.preferredSize.width = 60; bRepair.visible = false;
  bRepair.helpTip = "One-click fix: re-point the engine at this project's sidecar config";
  function refreshDoctor() {
    try {
      var d = MinColor.doctor();
      if (d.status === "yellow" && d.canRepair) {                    // filename-match self-heal (agreed 2026-08-27)
        try { MinColor.repair(); d = MinColor.doctor(); if (d.status === "green") log("auto-repaired config pin"); } catch (eAR) {}
      }
      var colors = { green: [0.28, 0.82, 0.4, 1], yellow: [0.95, 0.78, 0.18, 1], red: [0.94, 0.32, 0.28, 1], unmanaged: [0.55, 0.55, 0.55, 1] };
      dot.dotColor = colors[d.status] || colors.unmanaged;
      dot.helpTip = "Doctor: " + d.status + " \u2014 " + d.text + "  (click to re-check)";
      docText.text = d.text; bRepair.visible = (d.status === "yellow" && d.canRepair);
      syncEngineUI(d.status !== "unmanaged");
    } catch (e) { docText.text = "status unavailable: " + e; }
    unstick();
  }
  dot.onClick = function () { try { this.active = false; } catch (eA) {} refreshDoctor(); };
  bRepair.onClick = function () { guard("Repair", function () { return MinColor.repair(); }); };

  // ---- Set Up / Migrate dialog ----
  var pSetup = section("Setup Project", GLYPH.gear);
  var b = pSetup.add("button", undefined, "Set Up / Migrate Project…");
  var rowE = pSetup.add("group"); rowE.alignChildren = ["left", "center"];
  var stE = rowE.add("statictext", undefined, "Engine:");
  var ddEng = rowE.add("dropdownlist", undefined, ["Adobe", "minColor"]);
  ddEng.preferredSize.width = 110; ddEng.selection = 0;
  ddEng.helpTip = "Whose colour engine renders the minColor effects. minColor = our plugin: crash-proof preset switches, pinned OCIO 2.5 (needs the plugin installed everywhere). Adobe = AE's native OCIO effects: vanilla handoff, self-contained sidecar.";
  var engSyncing = false;
  function syncEngineUI(managed) {
    engSyncing = true;
    try {
      var e = MinColor.readEngine();
      var want = (e === "plugin") ? 1 : 0;                           // unmarked project reads as Adobe (the authoring default)
      if (!ddEng.selection || ddEng.selection.index !== want) ddEng.selection = want;   // reassigning inside onChange blanks the control — only touch it when wrong
      ddEng.enabled = (managed !== false);
    } catch (eE) {}
    engSyncing = false;
  }
  $.global.__mcEngineGo = function () {                              // runs OUTSIDE the dropdown's event context: doing the
    var target = $.global.__mcEngineTarget;                          // translate synchronously inside onChange blanks the control
    $.global.__mcEngineTarget = null;
    if (!target || MinColor.readEngine() === target) return;
    guard("Engine \u2192 " + target, function () {
      var r = MinColor.translateEffects(target);
      if (r.failed.length) alert("minColor \u2014 translate\n\n" + r.failed.join("\n").substr(0, 3000));
      return "converted " + r.converted.length + ", failed " + r.failed.length;
    });                                                              // guard's refreshDoctor re-syncs (and reverts on failure)
  };
  ddEng.onChange = function () {
    if (engSyncing || !ddEng.selection) return;
    var target = (ddEng.selection.index === 1) ? "plugin" : "native";
    if (MinColor.readEngine() === target) return;
    $.global.__mcEngineTarget = target;
    try { app.scheduleTask("if ($.global.__mcEngineGo) $.global.__mcEngineGo();", 60, false); }
    catch (eT) { $.global.__mcEngineGo(); }
  };
  var bArch = rowE.add("button", undefined, "Archive"); bArch.preferredSize.width = 70;
  bArch.helpTip = "Make the project self-contained for handoff/archival: sidecar config + provenance.json + a golden reference frame in _minColor/";
  bArch.onClick = function () {
    guard("Archive", function () {
      var r = MinColor.archiveProject();
      return "sidecar + provenance + " + r.golden;
    });
  };
  b.onClick = function () {
    var dlg = new Window("dialog", "minColor — Set Up / Migrate");
    dlg.orientation = "column"; dlg.alignChildren = ["fill", "top"]; dlg.margins = 12; dlg.spacing = 8;
    var r1 = dlg.add("group"); r1.add("statictext", undefined, "Working-space preset:");
    var dd = r1.add("dropdownlist", undefined, keys); dd.selection = 0;
    bindDD(dd, "setupPreset");
    dlg.add("statictext", undefined, "New Project: seeds a fresh project (asks where to save; sidecar created).");
    dlg.add("statictext", undefined, "Migrate Current: strips footage assignments (harvested as suggestions),");
    dlg.add("statictext", undefined, "sets working space + sidecar config, one save/backup/reopen.");
    var r2 = dlg.add("group"); r2.alignment = ["right", "top"];
    var bNew = r2.add("button", undefined, "New Project");
    var bMig = r2.add("button", undefined, "Migrate Current");
    r2.add("button", undefined, "Cancel", { name: "cancel" });
    var choice = null;
    bNew.onClick = function () { choice = "new"; dlg.close(1); };
    bMig.onClick = function () { choice = "migrate"; dlg.close(1); };
    if (dlg.show() !== 1 || !choice) return;
    if (choice === "new") {
      guard("New Project", function () { var r = MinColor.newProject(dd.selection.text, { interactive: true }); return "working=" + r.working; });
    } else {
      guard("Migrate", function () {
        var rep = MinColor.syncReport(); var bad = [];
        for (var i = 0; i < rep.rows.length; i++) if (!rep.rows[i].okFlag) bad.push("  " + rep.rows[i].current + "   " + rep.rows[i].item);
        var msg = "Preset: " + dd.selection.text + "\nFootage level becomes NOTHING." +
                  (bad.length ? "\nAssignments to strip (harvested first):\n" + bad.join("\n") : "\nNo assignments to strip.") +
                  "\n\nSave, back up, patch and reopen now?";
        if (!confirm(msg.substr(0, 3000))) return "cancelled";
        var r = MinColor.migrateProject(dd.selection.text);
        var warn = [];
        if (r.effectsFailed && r.effectsFailed.length) warn.push("CST REBUILD FAILED:\n  " + r.effectsFailed.join("\n  "));
        if (r.strippedPipeline && r.strippedPipeline.length) warn.push("REMOVED non-minColor OCIO pipeline effects (competing interpretation + crash risk; project is backed up):\n  " + r.strippedPipeline.join("\n  "));
        if (r.gradesLeft && r.gradesLeft.length) warn.push("OCIO CDL/File grades left in place (file-based \u2014 verify their look under the new working space):\n  " + r.gradesLeft.join("\n  "));
        if (r.orphanLayers && r.orphanLayers.length) warn.push("EMPTY minColor VIEW/RENDER layers (artifacts of an old bug \u2014 safe to delete):\n  " + r.orphanLayers.join("\n  "));
        if (warn.length) alert("minColor \u2014 migrate warnings\n\n" + warn.join("\n\n").substr(0, 3000));
        return "working=" + r.working + " stripped=" + r.stripped + " rebuilt=" + (r.effectsRebuilt || 0) + " view/render retargeted=" + (r.viewRenderRetargeted || 0) + " residual=" + r.residual + " | backups: " + (r.backups ? r.backups.count + " (" + r.backups.mb + " MB)" : "?");
      });
    }
  };

  // ---- Interpret (live) ----
  var pFtg = section("Interpret Footage", GLYPH.film);
  var rowS = pFtg.add("group"); rowS.add("statictext", undefined, "Selected as:");
  var ddSrc = rowS.add("dropdownlist", undefined, lists.inputSpaces); ddSrc.selection = 0;
  ddSrc.alignment = ["fill", "center"]; ddSrc.preferredSize.width = 150;
  var bSel = rowS.add("button", undefined, "Apply"); bSel.preferredSize.width = 60;
  bindDD(ddSrc, "interpretSpace");
  bSel.onClick = function () { runPass("Interpret selected", { mode: "selection" }, ddSrc.selection.text); };
  var pTL = section("Interpret Timeline", GLYPH.bars);
  var rowA = pTL.add("group");
  var bComp = rowA.add("button", undefined, "Interpret timeline"); bComp.alignment = ["fill", "center"];
  bComp.helpTip = "Active comp + nested precomps, auto-suggested per item";
  var bMatches = rowA.add("button", undefined, "Matches\u2026"); bMatches.preferredSize.width = 90;
  function runPass(label, scope, space) {
    guard(label, function () {
      var r = MinColor.interpretPass(scope, space || null);
      var detail = [];
      if (r.added.length) detail.push("ADDED:\n  " + r.added.join("\n  "));
      if (r.failed && r.failed.length) detail.push("FAILED:\n  " + r.failed.join("\n  "));
      if (r.flagged.length) detail.push("FLAGGED:\n  " + r.flagged.join("\n  "));
      if (r.identity && r.identity.length) detail.push("ALREADY WORKING-SPACE (identity, no CST needed):\n  " + r.identity.join("\n  "));
      if (r.skipped.length) detail.push("SKIPPED:\n  " + r.skipped.join("\n  "));
      if (detail.length) alert("minColor \u2014 " + label + "\n\n" + detail.join("\n\n").substr(0, 4000));
      return "added " + r.added.length + ", failed " + (r.failed ? r.failed.length : 0) + ", identity " + (r.identity ? r.identity.length : 0) + ", skipped " + r.skipped.length;
    });
  }
  bComp.onClick = function () { runPass("Interpret timeline", { mode: "comp" }); };
  bMatches.onClick = function () {
    var dlg = new Window("dialog", "minColor — Extension matches");
    dlg.orientation = "column"; dlg.alignChildren = ["fill", "top"]; dlg.margins = 12; dlg.spacing = 6;
    var lb = dlg.add("listbox", undefined, [], { numberOfColumns: 2, showHeaders: true, columnTitles: ["ext", "space"], columnWidths: [70, 200] });
    lb.preferredSize = [300, 180];
    function refreshMap() {
      lb.removeAll();
      var m = MinColor.extDefaults(), ks = [], k; for (k in m) ks.push(k); ks.sort();
      for (var i = 0; i < ks.length; i++) { var it = lb.add("item", ks[i]); it.subItems[0].text = m[ks[i]]; }
    }
    var row = dlg.add("group");
    var et = row.add("edittext", undefined, ""); et.characters = 6;
    var dds = row.add("dropdownlist", undefined, ["working (identity)"].concat(lists.inputSpaces)); dds.selection = 0; dds.preferredSize.width = 200;
    bindDD(dds, "matchesSpace");
    var row2 = dlg.add("group");
    var bSet = row2.add("button", undefined, "Add / Update");
    var bDel = row2.add("button", undefined, "Remove");
    row2.add("button", undefined, "Done", { name: "ok" });
    lb.onChange = function () { if (lb.selection) { et.text = lb.selection.text; var want = lb.selection.subItems[0].text; dds.selection = 0; for (var i = 1; i < dds.items.length; i++) if (dds.items[i].text === want) dds.selection = i; } };
    bSet.onClick = function () {
      try { var ext = et.text.replace(/^\./, "").toLowerCase(); if (!ext) throw new Error("type an extension");
        var m = MinColor.extDefaults(); m[ext] = (dds.selection.index === 0) ? "working" : dds.selection.text;
        MinColor.saveExtDefaults(m); refreshMap(); } catch (e) { alert(String(e)); }
    };
    bDel.onClick = function () { try { if (!lb.selection) throw new Error("select a row"); var m = MinColor.extDefaults(); delete m[lb.selection.text]; MinColor.saveExtDefaults(m); refreshMap(); } catch (e) { alert(String(e)); } };
    refreshMap(); dlg.show();
  };

  // ---- View ----
  var pAdj = section("Adjustment Layer", GLYPH.adj);
  var rowV = pAdj.add("group"); rowV.add("statictext", undefined, "View:");
  var ddView = rowV.add("dropdownlist", undefined, lists.viewSpaces); ddView.selection = 0;
  ddView.alignment = ["fill", "center"]; ddView.preferredSize.width = 150;
  bindDD(ddView, "viewSpace");
  var bView = rowV.add("button", undefined, "Add guide"); bView.preferredSize.width = 80;
  bView.helpTip = "Add / update the VIEW guide layer \u2014 viewport only, never renders";
  bView.onClick = function () {
    guard("View guide", function () {
      var comp = app.project.activeItem; if (!(comp instanceof CompItem)) throw new Error("open a comp");
      app.beginUndoGroup("minColor view guide");
      var r = MinColor.addViewGuideLayer(comp, ddView.selection.text);
      app.endUndoGroup();
      return ddView.selection.text + " (" + r.action + (r.disabledOther ? "; render layer switched off" : "") + ")";
    });
  };
  var renderSpaces = (function () { var seen = {}, out = [], all = lists.viewSpaces.concat(lists.inputSpaces); for (var ri = 0; ri < all.length; ri++) if (!seen[all[ri]]) { seen[all[ri]] = 1; out.push(all[ri]); } return out; })();
  var rowR = pAdj.add("group"); rowR.add("statictext", undefined, "Render:");
  var ddRender = rowR.add("dropdownlist", undefined, renderSpaces);
  ddRender.alignment = ["fill", "center"]; ddRender.preferredSize.width = 150;
  ddRender.selection = 0;
  for (var rdi = 0; rdi < ddRender.items.length; rdi++) if (ddRender.items[rdi].text === "Gamma 2.4 Encoded Rec.709") { ddRender.selection = rdi; break; }
  bindDD(ddRender, "renderSpace");
  var bRender = rowR.add("button", undefined, "Add render"); bRender.preferredSize.width = 80;
  bRender.helpTip = "Add / update the RENDER layer \u2014 renders in output: working \u2192 delivery space";
  bRender.helpTip = "Adjustment layer (NOT a guide \u2014 it renders): working space \u2192 delivery space";
  bRender.onClick = function () {
    guard("Render layer", function () {
      var comp = app.project.activeItem; if (!(comp instanceof CompItem)) throw new Error("open a comp");
      app.beginUndoGroup("minColor render layer");
      var r = MinColor.addRenderLayer(comp, ddRender.selection.text);
      app.endUndoGroup();
      return ddRender.selection.text + " (" + r.action + (r.disabledOther ? "; view guide switched off" : "") + ")";
    });
  };

  // ---- footer ----
  var rowF = win.add("group");
  var status = rowF.add("statictext", undefined, "ready \u00b7 v" + (MinColor.VERSION || "?"), { truncate: "end" }); status.alignment = ["fill", "center"];
  status.helpTip = "minColor v" + (MinColor.VERSION || "?");

  if (payloadError) { docText.text = "minColor payload not found — run the installer. (" + payloadError + ")"; }
  else refreshDoctor();
  // live Doctor: AE's scheduleTask re-checks every 5 s (6 ms a pop), so the lamp updates itself
  try { if ($.global.__minColorTask) app.cancelTask($.global.__minColorTask); } catch (eC) {}
  $.global.__minColorTick = function () { try { if (win.visible) refreshDoctor(); } catch (eK) {} };
  try { $.global.__minColorTask = app.scheduleTask("if ($.global.__minColorTick) $.global.__minColorTick();", 5000, true); } catch (eS) {}
  win.layout.layout(true); win.onResizing = win.onResize = function () { this.layout.resize(); };
  if (win instanceof Window) { win.center(); win.show(); }
 } catch (eTop) {   // never modal-block AE: log load errors instead
  try { var elog = new File(Folder.temp.fsName + "/minColor_panel_error.txt"); elog.open("w");
        elog.write(eTop.toString() + " line " + eTop.line); elog.close(); } catch (e2) {}
 }
})(this);
