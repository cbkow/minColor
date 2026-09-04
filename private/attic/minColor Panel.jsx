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
  var presets = {}, keys = [], lists = { inputSpaces: [], viewSpaces: [], renderSpaces: [], family: "Linear" }, payloadError = null, currentPreset = null, currentPin = null;
  try {
    presets = MinColor.presets(); for (var k in presets) keys.push(k);
    currentPreset = MinColor.currentPresetKey();                    // menus are PER PRESET; null = linear fallback lists
    lists = MinColor.menuLists(currentPreset);
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

  function showProjectDialog() {
    var dlg = new Window("dialog", "minColor \u2014 Project");
    dlg.orientation = "column"; dlg.alignChildren = ["fill", "top"]; dlg.margins = 14; dlg.spacing = 8;
    var d = MinColor.doctor();
    var inf = null; try { inf = MinColor.sidecarInfo(); } catch (e0) {}
    var eng = null; try { eng = MinColor.readEngine(); } catch (e1) {}
    var prov = null; try { prov = MinColor.readProvenance(); } catch (e2) {}
    var bk = null; try { bk = MinColor.doctor && MinColor.readEngine ? null : null; } catch (e3) {}
    function row(label, value) {
      var g = dlg.add("group"); g.alignChildren = ["left", "top"];
      var l = g.add("statictext", undefined, label); l.preferredSize.width = 80;
      var v = g.add("statictext", undefined, String(value).substr(0, 90), { truncate: "middle" }); v.preferredSize.width = 330;
    }
    row("Status", d.status + " \u2014 " + d.text);
    row("Engine", eng === "plugin" ? "minColor (plugin)" : eng === "native" ? "Adobe (native)" : "\u2014");
    row("Pinned to", (inf && inf.pinned) ? inf.pinned : "(no pin)");
    row("Provenance", prov || "(none)");
    var uisD = {}; try { uisD = MinColor.uiState() || {}; } catch (eU2) {}
    row("Last heal", uisD.lastHeal || "—");
    var rowB = dlg.add("group"); rowB.alignment = ["fill", "top"];
    var bArch2 = rowB.add("button", undefined, "Archive");
    bArch2.helpTip = "Freeze dependencies, keep working: sidecar + provenance.json + golden reference frame. Purely additive.";
    var bPkg2 = rowB.add("button", undefined, "Package for any AE");
    bPkg2.helpTip = "Deliberate exit: effects \u2192 Adobe, sidecar pinned, archive artifacts. Package a version increment, not your working copy.";
    var bAdopt2 = rowB.add("button", undefined, "Adopt minColor FX");
    bAdopt2.enabled = (eng === "native" && MinColor.pluginAvailable() && d.status !== "unmanaged");
    var managed = (d.status !== "unmanaged");
    bArch2.enabled = managed; bPkg2.enabled = managed;
    dlg.add("button", undefined, "Close", { name: "ok" });
    bArch2.onClick = function () { dlg.close(); guard("Archive", function () { var r = MinColor.archiveProject(); return "sidecar + provenance + " + r.golden; }); };
    bPkg2.onClick = function () { dlg.close(); guard("Package", function () { var r = MinColor.packageForAnyAE(); if (r.failed.length) alert("minColor \u2014 package\n\n" + r.failed.join("\n").substr(0, 3000)); return r.translated + " effect(s) \u2192 Adobe, sidecar pinned, " + r.archive.golden; }); };
    bAdopt2.onClick = function () { dlg.close(); guard("Adopt", function () { var r = MinColor.translateEffects("plugin"); return "adopted " + r.converted.length + " effect(s)"; }); };
    dlg.show();
  }

  // ---- remembered choices: dropdowns restore their last-used value and persist on change ----
  var UIS = {}; try { UIS = MinColor.uiState() || {}; } catch (eU) {}
  function bindDD(dd, key) {                                        // items may carry .key (preset keys) — persisted instead of the label
    if (UIS[key]) for (var bi = 0; bi < dd.items.length; bi++) if ((dd.items[bi].key || dd.items[bi].text) === UIS[key]) { dd.selection = bi; break; }
    if (dd.selection) dd.helpTip = dd.selection.text;
    dd.onChange = function () { if (dd.selection) { dd.helpTip = dd.selection.text; UIS[key] = dd.selection.key || dd.selection.text; try { MinColor.saveUiState(UIS); } catch (eS) {} } };
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
    precomp: function (g) {                                          /* comp-within-comp: the boundary made visible */
      g.newPath(); g.rectPath(3, 3.6, 12, 10.8); g.strokePath(pen(g, 1.4));
      g.newPath(); g.rectPath(7.2, 7.2, 5.6, 5.0); g.strokePath(pen(g, 1.2));
      g.newPath(); g.rectPath(8.6, 8.5, 2.8, 2.4); g.fillPath(brush(g));
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
  function flatButton(parent, label, opts) {                        // PILOT: owner-drawn flat button (modern-AE look)
    opts = opts || {};
    var b = parent.add("iconbutton", undefined, undefined, { style: "toolbutton" });
    b.textLabel = label; b.hov = false; b.dn = false;
    if (opts.width) { b.preferredSize = [opts.width, 24]; b.maximumSize = [opts.width, 24]; b.fixedW = opts.width; }
    else {
      b.preferredSize.height = 24; b.alignment = ["fill", "center"]; b.floorW = 70;
      try { b.floorW = Math.ceil(b.graphics.measureString(label, ScriptUI.newFont("dialog", opts.primary ? "BOLD" : "REGULAR", 11)).width) + 20; } catch (eM) {}
    }
    if (opts.tip) b.helpTip = opts.tip;
    b.onDraw = function () {
      var g = this.graphics, s = this.size;
      function pill(x, y, w, h, col) {                              /* capsule = caps + rect as ONE path, ONE fill —
                                                                       separate fills double-stack alpha at the overlaps */
        g.newPath();
        g.ellipsePath(x, y, h, h);
        g.ellipsePath(x + w - h, y, h, h);
        g.rectPath(x + h / 2, y, w - h, h);
        g.fillPath(g.newBrush(g.BrushType.SOLID_COLOR, col));
      }
      /* Palette discipline: AE's drawing pipe remaps non-neutral hues to theme colors, so the
         three states are built ONLY from reliables — grey fill, the theme-primary fill, and
         NEUTRAL outline (rim + near-panel center): filled = act, outlined = remove/undo. */
      var base = opts.primary ? [0.00784, 0.39608, 0.86275, 1] : [0.235, 0.235, 0.235, 1];  /* rgb(2,101,220) = Spectrum blue-900 accent-button fill */
      if (opts.outline) {
        var rimA = this.dn ? 0.55 : this.hov ? 0.45 : 0.32;
        pill(0, 0, s[0], s[1], [1, 1, 1, rimA]);                     /* rim */
        pill(1.5, 1.5, s[0] - 3, s[1] - 3, [0.13, 0.13, 0.13, 1]);   /* near-panel center = "transparent" */
      } else {
        var fill = this.dn ? [base[0] * 0.72, base[1] * 0.72, base[2] * 0.72, 1]
                 : this.hov ? [base[0] + 0.06, base[1] + 0.06, base[2] + 0.06, 1] : base;   /* equal bump = hue-preserving lighten (no green cast on grey) */
        /* border: on a PRIMARY (blue) button the rim is the accent itself (AE never rings the
           blue button in grey); a neutral button keeps a faint white rim for edge definition. */
        pill(0, 0, s[0], s[1], opts.primary ? [base[0], base[1], base[2], 1] : [1, 1, 1, 0.16]);
        pill(1, 1, s[0] - 2, s[1] - 2, fill);                          /* inset fill = clean 1px rim, no stroke seams */
      }
      var f = ScriptUI.newFont("dialog", opts.primary ? "BOLD" : "REGULAR", 11);
      var ts = g.measureString(this.textLabel, f);
      g.drawString(this.textLabel, g.newPen(g.PenType.SOLID_COLOR, [1, 1, 1, 1], 1),
                   Math.max(2, (s[0] - ts.width) / 2), Math.max(0, (s[1] - ts.height) / 2 - 1), f);
    };
    b.addEventListener("mouseover", function () { this.hov = true;  try { this.window.update(); } catch (e) {} });
    b.addEventListener("mouseout",  function () { this.hov = false; this.dn = false; try { this.window.update(); } catch (e) {} });
    b.addEventListener("mousedown", function () { this.dn = true;  try { this.window.update(); } catch (e) {} });
    b.addEventListener("mouseup",   function () { this.dn = false; try { this.window.update(); } catch (e) {} });
    return b;
  }
  /* Row fitting. ScriptUI's layout manager spreads a SHRINK across every child with room above
     its minimumSize (fixed pills reached 0 px at a 200 px dock, verified 2026-08-28), and setting
     minimumSize makes it double-count that width even at layout(true). So rows are placed by
     hand after every layout pass: fixed children keep their width, fill children split the rest
     (down to a text-derived floor, after which the row clips on the right like AE's own panels).
     Hidden children take no space (Repair no longer reserves a slot while hidden). */
  var ROWS = [];
  function hrow(parent) { var r = parent.add("group"); ROWS.push(r); return r; }
  function isFill(c) { var a = c.alignment; return (a instanceof Array) ? a[0] === "fill" : a === "fill"; }
  function fitRow(row) {
    var kids = row.children, sp = row.spacing, vis = [], fills = 0, fixed = 0, i, c;
    for (i = 0; i < kids.length; i++) { c = kids[i]; if (!c.visible) continue; vis.push(c); if (isFill(c)) fills++; else fixed += (c.fixedW || c.preferredSize[0]); }
    if (!vis.length) return;
    var share = fills ? (row.size[0] - fixed - sp * (vis.length - 1)) / fills : 0, x = 0;
    for (i = 0; i < vis.length; i++) {
      c = vis[i];
      var w = isFill(c) ? Math.max(c.floorW || 40, Math.floor(share)) : (c.fixedW || c.preferredSize[0]);
      c.size = [w, c.size[1]]; c.location = [x, c.location[1]]; x += w + sp;
    }
  }
  function fitRows() { for (var i = 0; i < ROWS.length; i++) try { fitRow(ROWS[i]); } catch (eF) {} }
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
  var rowDoc = hrow(win); rowDoc.alignChildren = ["left", "center"];
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
  var bRepair = flatButton(rowDoc, "Repair", { width: 60 }); bRepair.visible = false;
  var bProj = iconize(rowDoc.add("iconbutton", undefined, undefined, { style: "toolbutton" }), GLYPH.gear,
    "Project\u2026 \u2014 status, provenance, Archive / Package / Adopt");
  bProj.preferredSize = [20, 20]; bProj.minimumSize = [20, 20]; bProj.maximumSize = [20, 20];   // ScriptUI stretches toolbuttons unless hard-clamped
  bProj.alignment = ["right", "center"];                             // last in row + right-aligned = flush right (hidden Repair still reserves space otherwise)
  bProj.onClick = function () { try { showProjectDialog(); } catch (eP) { alert("minColor: " + eP); } };
  bRepair.helpTip = "One-click fix: re-point the engine at this project's sidecar config";
  function refreshDoctor() {
    try {
      var d = MinColor.doctor();
      if (d.status === "yellow" && d.canRepair) {                    // filename-match self-heal (agreed 2026-08-27)
        try {
          var oldPin = app.project.ocioConfigurationFile || "(empty)";
          MinColor.repair(); d = MinColor.doctor();
          if (d.status === "green") {
            log("auto-repaired config pin");
            $.global.__minColorHealTicks = 6;                        // status line announces for ~30 s of heartbeats
            var hn = new Date(), hz = function (n) { return (n < 10 ? "0" : "") + n; };
            UIS.lastHeal = hn.getFullYear() + "-" + hz(hn.getMonth() + 1) + "-" + hz(hn.getDate()) + " " +
                           hz(hn.getHours()) + ":" + hz(hn.getMinutes()) + " — " + oldPin + " → " +
                           app.project.ocioConfigurationFile;        // the gear dialog remembers, across sessions
            try { MinColor.saveUiState(UIS); } catch (eHS) {}
          }
        } catch (eAR) {}
      }
      var pk = d.preset || null, pin = d.pin || null;                // preset OR pin changed (open / migrate / new / closed / HEAL) -> menus follow the pinned config
      if (pk !== currentPreset || pin !== currentPin) { try { repopulateMenus(pk); currentPin = pin; } catch (eRp) {} }
      var colors = { green: [0.28, 0.82, 0.4, 1], yellow: [0.95, 0.78, 0.18, 1], red: [0.94, 0.32, 0.28, 1], unmanaged: [0.55, 0.55, 0.55, 1] };
      dot.dotColor = colors[d.status] || colors.unmanaged;
      dot.helpTip = "Doctor: " + d.status + " \u2014 " + d.text + "  (click to re-check)";
      docText.text = d.text;
      var showRepair = (d.status === "yellow" && d.canRepair);
      if (bRepair.visible !== showRepair) { bRepair.visible = showRepair; fitRow(rowDoc); }
      if ($.global.__minColorHealTicks > 0) { docText.text = d.text + "  · healed ✓"; $.global.__minColorHealTicks--; }
    } catch (e) { docText.text = "status unavailable: " + e; }
    unstick();
  }
  dot.onClick = function () { try { this.active = false; } catch (eA) {} refreshDoctor(); };
  bRepair.onClick = function () { guard("Repair", function () { return MinColor.repair(); }); };

  // ---- Set Up / Migrate dialog ----
  var pSetup = section("Setup Project", GLYPH.gear);
  var b = flatButton(pSetup, "Set Up / Migrate Project…");

  b.onClick = function () {
    var dlg = new Window("dialog", "minColor — Set Up / Migrate");
    dlg.orientation = "column"; dlg.alignChildren = ["fill", "top"]; dlg.margins = 14; dlg.spacing = 8;
    var hdr = dlg.add("group"); hdr.spacing = 6; hdr.alignChildren = ["left", "center"]; hdr.alignment = ["fill", "top"];
    var hic = hdr.add("iconbutton", undefined, undefined, { style: "toolbutton" }); hic.preferredSize = [18, 18];
    hic.onDraw = function () { GLYPH.gear(this.graphics); };
    var hst = hdr.add("statictext", undefined, "Set Up / Migrate");
    try { hst.graphics.font = ScriptUI.newFont("dialog", "BOLD", 11); } catch (eHd) {}
    var hln = hdr.add("panel"); hln.alignment = ["fill", "center"]; hln.preferredSize.height = 2; hln.minimumSize.width = 20;
    var r1 = dlg.add("group"); r1.add("statictext", undefined, "Working-space preset:");
    var dd = r1.add("dropdownlist", undefined, []);
    for (var pi = 0; pi < keys.length; pi++) { var itP = dd.add("item", presets[keys[pi]].label || keys[pi]); itP.key = keys[pi]; }
    dd.selection = 0;
    bindDD(dd, "setupPreset");
    dlg.add("statictext", undefined, "New Project: seeds a fresh project (asks where to save; sidecar created).");
    dlg.add("statictext", undefined, "Migrate Current: strips footage assignments (harvested as suggestions),");
    dlg.add("statictext", undefined, "sets working space + sidecar config, one save/backup/reopen.");
    var r2 = dlg.add("group"); r2.alignment = ["right", "top"];
    var bNew = flatButton(r2, "New Project", { width: 100 });
    var bMig = flatButton(r2, "Migrate Current", { width: 124, primary: true });
    var bCxl = flatButton(r2, "Cancel", { width: 72, outline: true });
    bCxl.onClick = function () { dlg.close(2); };
    try { dlg.cancelElement = bCxl; } catch (eCE) {}                 // keep Escape if ScriptUI accepts an iconbutton here
    var choice = null;
    bNew.onClick = function () { choice = "new"; dlg.close(1); };
    bMig.onClick = function () { choice = "migrate"; dlg.close(1); };
    if (dlg.show() !== 1 || !choice) return;
    if (choice === "new") {
      guard("New Project", function () { var r = MinColor.newProject(dd.selection.key, { interactive: true }); return "working=" + r.working; });
    } else {
      guard("Migrate", function () {
        var rep = MinColor.syncReport(); var bad = [];
        for (var i = 0; i < rep.rows.length; i++) if (!rep.rows[i].okFlag) bad.push("  " + rep.rows[i].current + "   " + rep.rows[i].item);
        var msg = "Preset: " + dd.selection.text + "\nFootage level becomes NOTHING." +
                  (bad.length ? "\nAssignments to strip (harvested first):\n" + bad.join("\n") : "\nNo assignments to strip.") +
                  "\n\nSave, back up, patch and reopen now?";
        if (!confirm(msg.substr(0, 3000))) return "cancelled";
        var activeName = (app.project.activeItem instanceof CompItem) ? app.project.activeItem.name : null;
        var r = MinColor.migrateProject(dd.selection.key, { utility: { compName: activeName, view: ddView.selection ? ddView.selection.text : null, render: ddRender.selection ? ddRender.selection.text : null } });
        try { repopulateMenus(dd.selection.key); } catch (eRp2) {}   // menus follow the new preset now, not at the next tick
        var warn = [];
        if (r.utility && r.utility.error) warn.push("VIEW/RENDER LAYERS: " + r.utility.error);
        if (r.effectsFailed && r.effectsFailed.length) warn.push("CST REBUILD FAILED:\n  " + r.effectsFailed.join("\n  "));
        if (r.effectsRemapped && r.effectsRemapped.length) warn.push("REMAPPED to this preset's spaces (review):\n  " + r.effectsRemapped.join("\n  "));
        if (r.effectsRemoved && r.effectsRemoved.length) warn.push("REMOVED \u2014 no equivalent in this preset (e.g. looks in SDR):\n  " + r.effectsRemoved.join("\n  "));
        if (r.strippedPipeline && r.strippedPipeline.length) warn.push("REMOVED non-minColor OCIO pipeline effects (competing interpretation + crash risk; project is backed up):\n  " + r.strippedPipeline.join("\n  "));
        if (r.gradesLeft && r.gradesLeft.length) warn.push("OCIO CDL/File grades left in place (file-based \u2014 verify their look under the new working space):\n  " + r.gradesLeft.join("\n  "));
        if (r.orphanLayers && r.orphanLayers.length) warn.push("EMPTY minColor VIEW/RENDER layers (artifacts of an old bug \u2014 safe to delete):\n  " + r.orphanLayers.join("\n  "));
        if (warn.length) alert("minColor \u2014 migrate warnings\n\n" + warn.join("\n\n").substr(0, 3000));
        var ut = r.utility && !r.utility.error ? " | " + r.utility.comp + ": view " + r.utility.view + ", render " + r.utility.render : "";
        return "working=" + r.working + (r.bitsPerChannel ? " · " + r.bitsPerChannel + " bpc" : "") + ut + " | pin: " + (r.pinLocus || "?") + " | stripped=" + r.stripped + " rebuilt=" + (r.effectsRebuilt || 0) + " remapped=" + (r.effectsRemapped ? r.effectsRemapped.length : 0) + " view/render retargeted=" + (r.viewRenderRetargeted || 0) + " residual=" + r.residual + " | backups: " + (r.backups ? r.backups.count + " (" + r.backups.mb + " MB)" : "?");
      });
    }
  };

  // ---- Interpret (live) ----
  var pFtg = section("Interpret Footage", GLYPH.film);
  var rowS = hrow(pFtg); rowS.add("statictext", undefined, "Selected as:");
  var ddSrc = rowS.add("dropdownlist", undefined, lists.inputSpaces); ddSrc.selection = 0;
  ddSrc.alignment = ["fill", "center"]; ddSrc.preferredSize.width = 150;
  var bSel = flatButton(rowS, "Apply", { width: 60 });
  bindDD(ddSrc, "interpretSpace");
  bSel.onClick = function () { runPass("Interpret selected", { mode: "selection" }, ddSrc.selection.text); };
  var pTL = section("Interpret Timeline", GLYPH.bars);
  var rowA = hrow(pTL);
  var bComp = flatButton(rowA, "Interpret timeline", { primary: true });
  bComp.helpTip = "Active comp + nested precomps, auto-suggested per item; contained precomps are treated as media";
  var bMatches = flatButton(rowA, "Matches\u2026", { width: 90 });
  var rowSt = hrow(pTL);
  var bStrip = flatButton(rowSt, "Strip foreign OCIO", { outline: true });
  var bStripAll = flatButton(rowSt, "Strip ALL", { width: 90, outline: true });
  bStripAll.helpTip = "DEMOLITION: remove EVERY OCIO effect from this timeline + precomps — foreign AND minColor, file grades included (listed), view/render layers deleted, containment ignored. One undo reverses it.";
  bStripAll.onClick = function () {
    if (!confirm("minColor — Strip ALL OCIO\n\nRemove EVERY OCIO effect from this timeline and its precomps?\n\n• foreign AND minColor effects\n• CDL/FILE grades too (listed in the report)\n• minColor view/render layers deleted\n• contained precomps NOT spared\n\nOne undo reverses everything.")) return;
    guard("Strip ALL OCIO", function () {
      app.beginUndoGroup("minColor strip ALL OCIO");
      var r = MinColor.stripForeignOcio(null, "all");
      app.endUndoGroup();
      var detail = [];
      if (r.stripped.length) detail.push("STRIPPED:\n  " + r.stripped.join("\n  "));
      if (r.layersRemoved.length) detail.push("LAYERS REMOVED:\n  " + r.layersRemoved.join("\n  "));
      if (r.failed.length) detail.push("FAILED:\n  " + r.failed.join("\n  "));
      if (detail.length) alert("minColor \u2014 strip ALL\n\n" + detail.join("\n\n").substr(0, 4000));
      return "stripped " + r.stripped.length + ", layers removed " + r.layersRemoved.length;
    });
  };
  bStrip.helpTip = "Remove non-minColor OCIO pipeline effects (Display/Look/CST) from this timeline + precomps — stale-config crash bait. File grades (CDL/FILE) and minColor effects stay; contained precomps are skipped. Undoable.";
  bStrip.onClick = function () {
    guard("Strip foreign OCIO", function () {
      app.beginUndoGroup("minColor strip foreign OCIO");
      var r = MinColor.stripForeignOcio();
      app.endUndoGroup();
      var detail = [];
      if (r.stripped.length) detail.push("STRIPPED:\n  " + r.stripped.join("\n  "));
      if (r.gradesLeft.length) detail.push("FILE GRADES LEFT (review):\n  " + r.gradesLeft.join("\n  "));
      if (r.contained.length) detail.push("CONTAINED (untouched):\n  " + r.contained.join("\n  "));
      if (r.failed.length) detail.push("FAILED:\n  " + r.failed.join("\n  "));
      if (detail.length) alert("minColor \u2014 strip\n\n" + detail.join("\n\n").substr(0, 4000));
      return "stripped " + r.stripped.length + ", grades left " + r.gradesLeft.length;
    });
  };

  /* Interpret Precomp (the `contain` grammar) left the panel 2026-08-29: too confusing as a UI;
     the grammar, the walk semantics and the library entry point stay for advanced/name-level use. */
  function runPass(label, scope, space) {
    guard(label, function () {
      var r = MinColor.interpretPass(scope, space || null);
      var detail = [];
      var cap = function (a) { return a.length <= 14 ? a.join("\n  ") : a.slice(0, 14).join("\n  ") + "\n  \u2026 and " + (a.length - 14) + " more"; };
      if (r.added.length) detail.push("ADDED:\n  " + cap(r.added));
      if (r.failed && r.failed.length) detail.push("FAILED:\n  " + cap(r.failed));
      if (r.flagged.length) detail.push("FLAGGED:\n  " + cap(r.flagged));
      if (r.identity && r.identity.length) detail.push("ALREADY WORKING-SPACE (identity, no CST needed):\n  " + cap(r.identity));
      if (r.contained && r.contained.length) detail.push("CONTAINED (treated as media, interior skipped):\n  " + cap(r.contained));
      if (r.skipped.length) detail.push("SKIPPED:\n  " + cap(r.skipped));
      if (detail.length) alert("minColor \u2014 " + label + "\n\n" + detail.join("\n\n"));   // per-bucket caps: a big ADDED list must never hide the skipped/identity buckets
      return "added " + r.added.length + ", failed " + (r.failed ? r.failed.length : 0) + ", identity " + (r.identity ? r.identity.length : 0) + ", skipped " + r.skipped.length;
    });
  }
  bComp.onClick = function () {
    runPass("Interpret timeline", { mode: "comp" });
    guard("View + Render", function () {                           // the timeline gets both utility layers from the current dropdowns; VIEW ends enabled
      var comp = app.project.activeItem; if (!(comp instanceof CompItem)) throw new Error("open a comp");
      app.beginUndoGroup("minColor view + render");
      var r = MinColor.ensureUtilityLayers(comp, ddView.selection ? ddView.selection.text : null, ddRender.selection ? ddRender.selection.text : null);
      app.endUndoGroup();
      return "view " + r.view + " (" + r.viewAction + "), render " + r.render + " (" + r.renderAction + ")";
    });
  };
  bMatches.onClick = function () {
    var dlg = new Window("dialog", "minColor — Extension matches");
    dlg.orientation = "column"; dlg.alignChildren = ["fill", "top"]; dlg.margins = 14; dlg.spacing = 8;
    var mh = dlg.add("group"); mh.spacing = 6; mh.alignChildren = ["left", "center"]; mh.alignment = ["fill", "top"];   // themed header, like the Setup dialog
    var mic = mh.add("iconbutton", undefined, undefined, { style: "toolbutton" }); mic.preferredSize = [18, 18];
    mic.onDraw = function () { GLYPH.bars(this.graphics); };
    var mst = mh.add("statictext", undefined, "Extension matches");
    try { mst.graphics.font = ScriptUI.newFont("dialog", "BOLD", 11); } catch (eMh) {}
    var mln = mh.add("panel"); mln.alignment = ["fill", "center"]; mln.preferredSize.height = 2; mln.minimumSize.width = 20;
    dlg.add("statictext", undefined, "Extension \u2192 colour space the Interpret passes assume. \"working\" = leave as is.");
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
    var row2 = dlg.add("group"); row2.alignment = ["right", "top"];
    var bSet = flatButton(row2, "Add / Update", { width: 104, primary: true });
    var bDel = flatButton(row2, "Remove", { width: 76, outline: true });
    var bDone = flatButton(row2, "Done", { width: 64 });
    try { dlg.defaultElement = bDone; } catch (eDe) {}
    lb.onChange = function () { if (lb.selection) { et.text = lb.selection.text; var want = lb.selection.subItems[0].text; dds.selection = 0; for (var i = 1; i < dds.items.length; i++) if (dds.items[i].text === want) dds.selection = i; } };
    function applyRule(quiet) {
      try { var ext = et.text.replace(/^\./, "").toLowerCase(); if (!ext) { if (quiet) return; throw new Error("type an extension"); }
        var m = MinColor.extDefaults(); m[ext] = (dds.selection.index === 0) ? "working" : dds.selection.text;
        MinColor.saveExtDefaults(m); refreshMap(); } catch (e) { alert(String(e)); }
    }
    bSet.onClick = function () { applyRule(false); };
    bDone.onClick = function () {                                    // Done commits a pending edit instead of discarding it —
      try {                                                          // pressing Done without Add/Update was silently losing the rule
        var ext = et.text.replace(/^\./, "").toLowerCase();
        if (ext) { var m = MinColor.extDefaults(); var want = (dds.selection.index === 0) ? "working" : dds.selection.text;
          if (m[ext] !== want) applyRule(true); }
      } catch (eD) {}
      dlg.close(1);
    };
    bDel.onClick = function () { try { if (!lb.selection) throw new Error("select a row"); var m = MinColor.extDefaults(); delete m[lb.selection.text]; MinColor.saveExtDefaults(m); refreshMap(); } catch (e) { alert(String(e)); } };
    refreshMap(); dlg.show();
  };

  // ---- View ----
  var pAdj = section("Adjustment Layer", GLYPH.adj);
  var rowV = hrow(pAdj); rowV.add("statictext", undefined, "View:");
  var ddView = rowV.add("dropdownlist", undefined, lists.viewSpaces); ddView.selection = 0;
  ddView.alignment = ["fill", "center"]; ddView.preferredSize.width = 150;
  bindDD(ddView, "viewSpace");
  var bView = flatButton(rowV, "Apply", { width: 80, primary: true });
  bView.helpTip = "Apply this VIEW to the comp's guide layer \u2014 viewport only, never renders";
  bView.onClick = function () {
    guard("View guide", function () {
      var comp = app.project.activeItem; if (!(comp instanceof CompItem)) throw new Error("open a comp");
      app.beginUndoGroup("minColor view guide");
      var r = MinColor.addViewGuideLayer(comp, ddView.selection.text);
      app.endUndoGroup();
      return ddView.selection.text + " (" + r.action + (r.disabledOther ? "; render layer switched off" : "") + ")";
    });
  };
  var rowR = hrow(pAdj); rowR.add("statictext", undefined, "Render:");
  var ddRender = rowR.add("dropdownlist", undefined, lists.renderSpaces);   // per preset; view-only spaces never appear here
  ddRender.alignment = ["fill", "center"]; ddRender.preferredSize.width = 150;
  ddRender.selection = 0;
  var renderDefault = MinColor.familyDefaults(lists.family).render;        // Linear: Gamma 2.4 Rec.709 · Display (SDR): Rec.1886 — never auto-added
  for (var rdi = 0; rdi < ddRender.items.length; rdi++) if (ddRender.items[rdi].text === renderDefault) { ddRender.selection = rdi; break; }
  bindDD(ddRender, "renderSpace");
  var bRender = flatButton(rowR, "Apply", { width: 80, primary: true });
  bRender.helpTip = "Apply this RENDER to the comp's render layer (NOT a guide \u2014 it renders): working space \u2192 delivery space";
  bRender.onClick = function () {
    guard("Render layer", function () {
      var comp = app.project.activeItem; if (!(comp instanceof CompItem)) throw new Error("open a comp");
      app.beginUndoGroup("minColor render layer");
      var r = MinColor.addRenderLayer(comp, ddRender.selection.text);
      app.endUndoGroup();
      return ddRender.selection.text + " (" + r.action + (r.disabledOther ? "; view guide switched off" : "") + ")";
    });
  };

  var lookNames = (function () { try { return MinColor.configLooks(); } catch (eL) { return []; } })();
  {                                                                  // Look row ALWAYS exists; disabled when the preset has no looks (SDR family) — repopulateMenus toggles it
    var rowL = hrow(pAdj); rowL.add("statictext", undefined, "Look:");
    var ddLook = rowL.add("dropdownlist", undefined, ["(none)"].concat(lookNames)); ddLook.selection = 0;
    ddLook.alignment = ["fill", "center"]; ddLook.preferredSize.width = 150;
    bindDD(ddLook, "lookChoice");
    rowL.enabled = lookNames.length > 0;
    var bLook = flatButton(rowL, "Apply", { width: 80 });
    bLook.helpTip = "Set or remove the look on the existing view/render layers (applies before the display transform; spaces untouched)";
    bLook.onClick = function () {
      guard("Look", function () {
        app.beginUndoGroup("minColor look");
        var lk = (ddLook.selection.index === 0) ? null : ddLook.selection.text;
        var r = MinColor.applyLook(lk);
        app.endUndoGroup();
        return (lk || "none") + " (view " + r.view + ", render " + r.render + ")";
      });
    };
  }
  var presetNames = (function () { var ks = [], k, m = MinColor.renderPresets(); for (k in m) ks.push(k); ks.sort(); return ks; })();
  if (presetNames.length) {
    var rowP = hrow(pAdj); rowP.add("statictext", undefined, "Preset:");
    var ddPreset = rowP.add("dropdownlist", undefined, presetNames); ddPreset.selection = 0;
    ddPreset.alignment = ["fill", "center"]; ddPreset.preferredSize.width = 150;
    bindDD(ddPreset, "renderPreset");
    var bPreset = flatButton(rowP, "Apply", { width: 80 });
    bPreset.helpTip = "Apply a render preset recipe: rewrites BOTH view and render layers (look + spaces); view stays enabled";
    bPreset.onClick = function () {
      guard("Render preset", function () {
        app.beginUndoGroup("minColor render preset");
        var r = MinColor.applyRenderPreset(ddPreset.selection.text);
        app.endUndoGroup();
        var lk = (r.view.lookAction !== "none" || r.render.lookAction !== "none") ? ("; look " + r.view.lookAction) : "";
        return r.preset + " (view " + r.view.action + ", render " + r.render.action + lk + ")";
      });
    };
  }

  // ---- per-preset menus: dropdowns follow the project's preset (Doctor detects the change) ----
  function refill(dd, items, key, preferred, initial) {             // swap items; keep current -> persisted -> family default -> first (initial: no "current" yet)
    var cur = (!initial && dd.selection) ? dd.selection.text : null, onCh = dd.onChange; dd.onChange = null;
    dd.removeAll(); for (var i = 0; i < items.length; i++) dd.add("item", items[i]);
    var want = [cur, UIS[key], preferred], sel = -1;
    for (var w = 0; w < want.length && sel < 0; w++) if (want[w]) for (var j = 0; j < dd.items.length; j++) if (dd.items[j].text === want[w]) { sel = j; break; }
    if (dd.items.length) dd.selection = (sel < 0 ? 0 : sel);
    if (dd.selection) dd.helpTip = dd.selection.text;
    dd.onChange = onCh;                                              // UIS is NOT written here: only user changes persist
  }
  function repopulateMenus(presetKey, initial) {
    lists = MinColor.menuLists(presetKey);                           // the Matches dialog reads `lists` lazily, so it follows too
    var fd = MinColor.familyDefaults(lists.family);
    refill(ddSrc, lists.inputSpaces, "interpretSpace", null, initial);
    refill(ddView, lists.viewSpaces, "viewSpace", fd.view, initial);
    refill(ddRender, lists.renderSpaces, "renderSpace", fd.render, initial);
    if (typeof ddLook !== "undefined" && ddLook) {                   // display-referred presets have no looks: row stays, disabled
      var lk = []; try { lk = MinColor.configLooks(); } catch (eL2) {}
      refill(ddLook, ["(none)"].concat(lk), "lookChoice", null);
      rowL.enabled = lk.length > 0;
    }
    currentPreset = presetKey;
    fitRows();
  }

  repopulateMenus(currentPreset, true);                             // first-use defaults apply when nothing (or a stale name) is persisted

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
  win.layout.layout(true); fitRows();
  win.onResizing = win.onResize = function () { this.layout.resize(); fitRows(); };
  if (win instanceof Window) { win.center(); win.show(); }
 } catch (eTop) {   // never modal-block AE: log load errors instead
  try { var elog = new File(Folder.temp.fsName + "/minColor_panel_error.txt"); elog.open("w");
        elog.write(eTop.toString() + " line " + eTop.line); elog.close(); } catch (e2) {}
 }
})(this);
