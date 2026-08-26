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
    try { var r = fn(); log(label + ": " + (r === undefined ? "ok" : r)); refreshDoctor(); }
    catch (e) { log(label + " FAILED: " + e.toString()); alert("minColor — " + label + " failed:\n" + e.toString()); unstick(); }
  }

  // ---- Doctor status line ----
  var rowDoc = win.add("group"); rowDoc.alignChildren = ["left", "center"];
  var dot = rowDoc.add("statictext", undefined, "●"); dot.preferredSize.width = 14;
  var docText = rowDoc.add("statictext", undefined, "…", { truncate: "end" }); docText.alignment = ["fill", "center"];
  var bRepair = rowDoc.add("button", undefined, "Repair"); bRepair.preferredSize.width = 60; bRepair.visible = false;
  var bRefresh = rowDoc.add("button", undefined, "Recheck"); bRefresh.preferredSize.width = 70;
  function refreshDoctor() {
    try {
      var d = MinColor.doctor();
      var colors = { green: [0.3, 0.85, 0.35], yellow: [0.95, 0.8, 0.2], red: [0.95, 0.35, 0.3], unmanaged: [0.6, 0.6, 0.6] };
      try { dot.graphics.foregroundColor = dot.graphics.newPen(dot.graphics.PenType.SOLID_COLOR, colors[d.status] || colors.unmanaged, 1); } catch (e) {}
      docText.text = d.text; bRepair.visible = (d.status === "yellow" && d.canRepair);
    } catch (e) { docText.text = "status unavailable: " + e; }
    unstick();
  }
  bRefresh.onClick = function () { try { this.active = false; } catch (eA) {} refreshDoctor(); };
  bRepair.onClick = function () { guard("Repair", function () { return MinColor.repair(); }); };

  // ---- Set Up / Migrate dialog ----
  var pSetup = win.add("panel", undefined, "Setup Project");
  pSetup.orientation = "column"; pSetup.alignChildren = ["fill", "top"]; pSetup.margins = 8; pSetup.spacing = 4;
  var b = pSetup.add("button", undefined, "Set Up / Migrate Project…");
  b.onClick = function () {
    var dlg = new Window("dialog", "minColor — Set Up / Migrate");
    dlg.orientation = "column"; dlg.alignChildren = ["fill", "top"]; dlg.margins = 12; dlg.spacing = 8;
    var r1 = dlg.add("group"); r1.add("statictext", undefined, "Working-space preset:");
    var dd = r1.add("dropdownlist", undefined, keys); dd.selection = 0;
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
        return "working=" + r.working + " stripped=" + r.stripped + " residual=" + r.residual;
      });
    }
  };

  // ---- Interpret (live) ----
  var pFtg = win.add("panel", undefined, "Interpret Footage");
  pFtg.orientation = "column"; pFtg.alignChildren = ["fill", "top"]; pFtg.margins = 8; pFtg.spacing = 4;
  var rowS = pFtg.add("group"); rowS.add("statictext", undefined, "Selected as:");
  var ddSrc = rowS.add("dropdownlist", undefined, lists.inputSpaces); ddSrc.selection = 0; ddSrc.preferredSize.width = 220;
  var bSel = rowS.add("button", undefined, "Apply"); bSel.preferredSize.width = 60;
  bSel.onClick = function () { runPass("Interpret selected", { mode: "selection" }, ddSrc.selection.text); };
  var pTL = win.add("panel", undefined, "Interpret Timeline");
  pTL.orientation = "column"; pTL.alignChildren = ["fill", "top"]; pTL.margins = 8; pTL.spacing = 4;
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
  var pAdj = win.add("panel", undefined, "Adjustment Layer");
  pAdj.orientation = "column"; pAdj.alignChildren = ["fill", "top"]; pAdj.margins = 8; pAdj.spacing = 4;
  var rowV = pAdj.add("group"); rowV.add("statictext", undefined, "View:");
  var ddView = rowV.add("dropdownlist", undefined, lists.viewSpaces); ddView.selection = 0; ddView.preferredSize.width = 220;
  var bView = rowV.add("button", undefined, "Add guide"); bView.preferredSize.width = 80;
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
  var ddRender = rowR.add("dropdownlist", undefined, renderSpaces); ddRender.preferredSize.width = 220;
  ddRender.selection = 0;
  for (var rdi = 0; rdi < ddRender.items.length; rdi++) if (ddRender.items[rdi].text === "Gamma 2.4 Encoded Rec.709") { ddRender.selection = rdi; break; }
  var bRender = rowR.add("button", undefined, "Add render"); bRender.preferredSize.width = 80;
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
  var status = rowF.add("statictext", undefined, "ready", { truncate: "end" }); status.alignment = ["fill", "center"];

  if (payloadError) { docText.text = "minColor payload not found — run the installer. (" + payloadError + ")"; }
  else refreshDoctor();
  win.layout.layout(true); win.onResizing = win.onResize = function () { this.layout.resize(); };
  if (win instanceof Window) { win.center(); win.show(); }
 } catch (eTop) {   // never modal-block AE: log load errors instead
  try { var elog = new File(Folder.temp.fsName + "/minColor_panel_error.txt"); elog.open("w");
        elog.write(eTop.toString() + " line " + eTop.line); elog.close(); } catch (e2) {}
 }
})(this);
