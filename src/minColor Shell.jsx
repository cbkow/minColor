// minColor Shell.jsx — M3: the THIN shell. Zero library: every behavior is a native menu
// command; this file is buttons, dropdowns, report rendering and the hard gate. The union
// pill theme is ported verbatim from minColor Panel.jsx (all gotchas field-verified there).
// Contract: settings/aegp-api.json is the gate; commands are driven through shell-args.json
// ({command, silent:true, ...args}) and answered via settings/reports/<name>-last.json.

(function (thisObj) {
 try {
  var win;
  if (thisObj instanceof Panel) win = thisObj;
  else {
    try { if ($.global.__minColorWin) $.global.__minColorWin.close(); } catch (e) {}
    win = new Window("palette", "minColor", undefined, { resizeable: true });
    $.global.__minColorWin = win;
  }
  win.orientation = "column"; win.alignChildren = ["fill", "top"]; win.spacing = 6; win.margins = 10;

  // ---- shared paths + tiny JSON I/O (the shell's only "library") ----
  var SETTINGS = ($.os.indexOf("Windows") >= 0) ? "C:/ProgramData/minColor/settings" : "/Users/Shared/minColor/settings";
  function readJSON(path) {
    var f = new File(path); if (!f.exists || !f.open("r")) return null;
    var s = f.read(); f.close();
    try { return eval("(" + s + ")"); } catch (e) { return null; }
  }
  function writeText(path, s) {
    var f = new File(path); f.encoding = "UTF-8"; f.lineFeed = "Unix";
    if (!f.open("w")) return false;
    f.write(s); f.close(); return true;
  }
  function jstr(s) { return '"' + String(s).replace(/\\/g, "\\\\").replace(/"/g, '\\"') + '"'; }

  // ---- HARD GATE: the handshake is the contract (first-ever consumer) ----
  var REQUIRED = ["minColor: Sync From Names", "minColor: Doctor", "minColor: Interpret Timeline",
                  "minColor: Interpret Selected", "minColor: Set Up Project", "minColor: Migrate Project",
                  "minColor: Strip Foreign OCIO", "minColor: Strip ALL", "minColor: Archive Project",
                  "minColor: Package for Any AE", "minColor: Utility Layers", "minColor: Apply Look",
                  "minColor: Adopt Effects", "minColor: Repair", "minColor: Apply Render Preset"];
  function gateCheck() {
    var hs = readJSON(SETTINGS + "/aegp-api.json");
    if (!hs) return { ok: false, why: "engine not found (no handshake file)" };
    if (hs.apiVersion !== 1) return { ok: false, why: "engine API v" + hs.apiVersion + " (this shell needs v1)", hs: hs };
    var have = {}; for (var i = 0; i < (hs.commands || []).length; i++) have[hs.commands[i]] = 1;
    for (var r = 0; r < REQUIRED.length; r++)
      if (!have[REQUIRED[r]]) return { ok: false, why: "engine out of date — missing '" + REQUIRED[r] + "'", hs: hs };
    return { ok: true, hs: hs };
  }
  var GATE = gateCheck();
  if (!GATE.ok) {                                     // gate-only panel: message + versions + Retry
    var gs = win.add("statictext", undefined, "minColor engine not available", { truncate: "end" });
    try { gs.graphics.font = ScriptUI.newFont("dialog", "BOLD", 12); } catch (eGF) {}
    win.add("statictext", undefined, GATE.why, { truncate: "end" });
    win.add("statictext", undefined, "Install the minColor 2.0 plug-ins and restart After Effects.", { truncate: "end" });
    if (GATE.hs) win.add("statictext", undefined, "found: " + (GATE.hs.version || "?") + " (" + (GATE.hs.buildStamp || "?") + ")", { truncate: "end" });
    var bRetry = win.add("button", undefined, "Retry");
    bRetry.onClick = function () { alert("minColor: restart After Effects after installing — the engine registers at launch."); };
    win.layout.layout(true);
    if (win instanceof Window) { win.center(); win.show(); }
    return;
  }
  var ENGINE = GATE.hs;                               // {version, buildStamp, commands}

  function log(s) { status.text = String(s).substr(0, 240); }
  function unstick() { try { win.update(); } catch (e3) {} }

  // ---- command runner: shell-args -> executeCommand -> report ----
  function runCmd(label, args, reportName) {
    var json = '{ "command": ' + jstr(label) + ', "silent": true';
    if (args) for (var k in args) if (args[k] !== null && args[k] !== undefined) json += ', ' + jstr(k) + ': ' + jstr(args[k]);
    json += ' }';
    writeText(SETTINGS + "/shell-args.json", json + "\n");
    var id = app.findMenuCommandId(label);
    if (!id) throw new Error("command not registered: " + label);
    app.executeCommand(id);
    if (!reportName) return null;
    var r = readJSON(SETTINGS + "/reports/" + reportName + "-last.json");
    if (r && r.error) throw new Error(r.error);
    return r;
  }
  function cap(a) { return a.length <= 14 ? a.join("\n  ") : a.slice(0, 14).join("\n  ") + "\n  \u2026 and " + (a.length - 14) + " more"; }
  function guard(label, fn) {
    try { log(label + "\u2026"); unstick(); } catch (eB) {}
    try { var r = fn(); log(label + ": " + (r === undefined ? "ok" : r)); refreshDoctor(); }
    catch (e) { log(label + " FAILED: " + e.toString()); alert("minColor \u2014 " + label + " failed:\n" + e.toString()); unstick(); }
  }

  // ---- dropdown feeds: plugin-menus.json (AEGP-written; menus follow the pin) ----
  var MENUS = { inputSpaces: [], viewSpaces: [], renderSpaces: [], looks: [], family: "Linear", defaultView: null, defaultRender: null, preset: null };
  function loadMenus() { var m = readJSON(SETTINGS + "/plugin-menus.json"); if (m) MENUS = m; return !!m; }
  loadMenus();
  function renderPresetNames() {
    var m = readJSON(SETTINGS + "/render-presets.json"), ks = [], k;
    if (m && m.presets) for (k in m.presets) ks.push(k);
    ks.sort(); return ks;
  }

  // ---- remembered choices (same keys as the panel: users keep their stickies) ----
  var UIS = readJSON(SETTINGS + "/ui-state.json") || {};
  function saveUIS() {
    var parts = [], k;
    for (k in UIS) if (UIS[k] !== null && UIS[k] !== undefined) parts.push(jstr(k) + ": " + jstr(UIS[k]));
    writeText(SETTINGS + "/ui-state.json", "{ " + parts.join(", ") + " }\n");
  }
  function bindDD(dd, key) {
    if (UIS[key]) for (var bi = 0; bi < dd.items.length; bi++) if ((dd.items[bi].key || dd.items[bi].text) === UIS[key]) { dd.selection = bi; break; }
    if (dd.selection) dd.helpTip = dd.selection.text;
    dd.onChange = function () { if (dd.selection) { dd.helpTip = dd.selection.text; UIS[key] = dd.selection.key || dd.selection.text; try { saveUIS(); } catch (eS) {} } };
  }

  // ---- flat-icon toolkit (VERBATIM port from minColor Panel.jsx — all gotchas field-verified) ----
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
  function flatButton(parent, label, opts) {
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
      var base = opts.primary ? [0.24, 0.40, 0.90, 1] : [0.235, 0.235, 0.235, 1];
      if (opts.outline) {
        var rimA = this.dn ? 0.55 : this.hov ? 0.45 : 0.32;
        pill(0, 0, s[0], s[1], [1, 1, 1, rimA]);                     /* rim */
        pill(1.5, 1.5, s[0] - 3, s[1] - 3, [0.13, 0.13, 0.13, 1]);   /* near-panel center = "transparent" */
      } else {
        var fill = this.dn ? [base[0] * 0.72, base[1] * 0.72, base[2] * 0.72, 1]
                 : this.hov ? [base[0] + 0.06, base[1] + 0.06, base[2] + 0.06, 1] : base;
        pill(0, 0, s[0], s[1], [1, 1, 1, opts.primary ? 0.25 : 0.16]); /* border layer */
        pill(1, 1, s[0] - 2, s[1] - 2, fill);                          /* inset fill = clean 1px rim, no stroke seams */
      }
      var f = ScriptUI.newFont("dialog", opts.primary ? "BOLD" : "REGULAR", 11);
      var ts = g.measureString(this.textLabel, f);
      g.drawString(this.textLabel, g.newPen(g.PenType.SOLID_COLOR, [0.95, 0.95, 0.95, 1], 1),
                   Math.max(2, (s[0] - ts.width) / 2), Math.max(0, (s[1] - ts.height) / 2 - 1), f);
    };
    b.addEventListener("mouseover", function () { this.hov = true;  try { this.window.update(); } catch (e) {} });
    b.addEventListener("mouseout",  function () { this.hov = false; this.dn = false; try { this.window.update(); } catch (e) {} });
    b.addEventListener("mousedown", function () { this.dn = true;  try { this.window.update(); } catch (e) {} });
    b.addEventListener("mouseup",   function () { this.dn = false; try { this.window.update(); } catch (e) {} });
    return b;
  }
  /* Row fitting — see the panel's comment of record: ScriptUI spreads shrink across children
     with room above minimumSize (0 px pills at a 200 px dock), and minimumSize double-counts. */
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
  function section(title, glyph) {
    var hdr = win.add("group"); hdr.spacing = 6; hdr.alignChildren = ["left", "center"]; hdr.alignment = ["fill", "top"]; hdr.margins = [0, 6, 0, 0];
    var ic = hdr.add("iconbutton", undefined, undefined, { style: "toolbutton" }); ic.preferredSize = [18, 18];
    ic.onDraw = function () { glyph(this.graphics); };
    var st = hdr.add("statictext", undefined, title);
    try { st.graphics.font = ScriptUI.newFont("dialog", "BOLD", 11); } catch (eH) {}
    var ln = hdr.add("panel"); ln.alignment = ["fill", "center"]; ln.preferredSize.height = 2; ln.minimumSize.width = 20;
    var body = win.add("group"); body.orientation = "column"; body.alignChildren = ["fill", "top"]; body.spacing = 4; body.margins = [22, 0, 0, 2];
    return body;
  }

  // ---- Doctor row: silent native Doctor -> report; live heal on yellow+repairTarget ----
  var rowDoc = hrow(win); rowDoc.alignChildren = ["left", "center"];
  var dot = rowDoc.add("iconbutton", undefined, undefined, { style: "toolbutton" });
  dot.preferredSize = [18, 18]; dot.dotColor = [0.6, 0.6, 0.6, 1];
  dot.helpTip = "minColor Doctor \u2014 click to re-check now (auto-checks every 5 s)";
  dot.onDraw = function () {
    var g = this.graphics;
    g.newPath(); g.ellipsePath(3, 3, 12, 12);
    g.fillPath(g.newBrush(g.BrushType.SOLID_COLOR, this.dotColor));
    g.strokePath(g.newPen(g.PenType.SOLID_COLOR, [0, 0, 0, 0.45], 1));
    g.newPath(); g.ellipsePath(5.5, 5, 5, 3.4);
    g.fillPath(g.newBrush(g.BrushType.SOLID_COLOR, [1, 1, 1, 0.28]));
  };
  var docText = rowDoc.add("statictext", undefined, "\u2026", { truncate: "end" }); docText.alignment = ["fill", "center"];
  var bRepair = flatButton(rowDoc, "Repair", { width: 60 }); bRepair.visible = false;
  bRepair.helpTip = "One-click fix: re-point the engine at this project's local config copy";
  var bProj = iconize(rowDoc.add("iconbutton", undefined, undefined, { style: "toolbutton" }), GLYPH.gear,
    "Project\u2026 \u2014 status, Archive / Package / Adopt");
  bProj.preferredSize = [20, 20]; bProj.minimumSize = [20, 20]; bProj.maximumSize = [20, 20];
  bProj.alignment = ["right", "center"];

  var currentPreset = null, currentPin = null;
  function doctorNow(passive) {
    if (!passive) return runCmd("minColor: Doctor", {}, "doctor");   // user-initiated: run the command
    /* heartbeat is READ-ONLY: the AEGP diagnoses on idle and writes doctor-last.json on
       change — a panel timer must NEVER executeCommand (AE dispatch mid-startup or
       mid-project-load throws script errors and can kill the panel's drawing).         */
    var f = new File(SETTINGS + "/reports/doctor-last.json");
    if (!f.exists || !f.open("r")) return null;
    var s = f.read(); f.close();
    var changed = (s !== $.global.__minColorDocRaw);
    $.global.__minColorDocRaw = s;
    var d; try { d = eval("(" + s + ")"); } catch (eP) { return null; }
    if (d) d.__fresh = changed;                       // heal only on TRANSITION, never every tick
    return d;
  }
  function liveHeal(d) {                              // the panel's auto-heal, engine-computed target
    var oldPin = app.project.ocioConfigurationFile || "(empty)";
    app.project.colorManagementSystem = 1;            // (CMS enum: OCIO)
    app.project.ocioConfigurationFile = d.repairTarget;
    app.purge(PurgeTarget.ALL_CACHES);                // cached frames predate the heal
    runCmd("minColor: Sync From Names", {});          // refresh the effects against the healed authority
    var after = doctorNow();
    if (after && after.status === "green") {
      log("auto-repaired config pin");
      $.global.__minColorHealTicks = 6;
      var hn = new Date(), hz = function (n) { return (n < 10 ? "0" : "") + n; };
      UIS.lastHeal = hn.getFullYear() + "-" + hz(hn.getMonth() + 1) + "-" + hz(hn.getDate()) + " " +
                     hz(hn.getHours()) + ":" + hz(hn.getMinutes()) + " \u2014 " + oldPin + " \u2192 " +
                     app.project.ocioConfigurationFile;
      try { saveUIS(); } catch (eHS) {}
    }
    return after || d;
  }
  function refreshDoctor(passive) {
    try {
      var d = doctorNow(passive);
      if (!d) { if (!passive) docText.text = "no doctor report"; return; }
      if (d.status === "yellow" && d.repairTarget && (!passive || d.__fresh)) { try { d = liveHeal(d); } catch (eAR) {} }
      var pk = d.preset || null, pin = d.pin || null;
      if (pk !== currentPreset || pin !== currentPin) { try { repopulateMenus(); currentPreset = pk; currentPin = pin; } catch (eRp) {} }
      var colors = { green: [0.28, 0.82, 0.4, 1], yellow: [0.95, 0.78, 0.18, 1], red: [0.94, 0.32, 0.28, 1], unmanaged: [0.55, 0.55, 0.55, 1] };
      dot.dotColor = colors[d.status] || colors.unmanaged;
      dot.helpTip = "Doctor: " + d.status + " \u2014 " + d.text + "  (click to re-check)";
      docText.text = d.text;
      var showRepair = (d.status === "yellow" && d.canRepair);
      if (bRepair.visible !== showRepair) { bRepair.visible = showRepair; fitRow(rowDoc); }
      if ($.global.__minColorHealTicks > 0) { docText.text = d.text + "  \u00b7 healed \u2713"; $.global.__minColorHealTicks--; }
    } catch (e) { docText.text = "status unavailable: " + e; }
    unstick();
  }
  dot.onClick = function () { try { this.active = false; } catch (eA) {} refreshDoctor(); };
  bRepair.onClick = function () {
    guard("Repair", function () {
      var d = doctorNow();
      if (d && d.repairTarget) { liveHeal(d); return "healed"; }
      var r = runCmd("minColor: Repair", {}, "repair");   // shell-less twin as fallback (reopens)
      return r.status + (r.repairedTo ? " \u2190 " + r.repairedTo : "");
    });
  };

  // ---- Project dialog (gear): Archive / Package / Adopt via commands ----
  bProj.onClick = function () {
    try {
      var dlg = new Window("dialog", "minColor \u2014 Project");
      dlg.orientation = "column"; dlg.alignChildren = ["fill", "top"]; dlg.margins = 14; dlg.spacing = 8;
      var d = doctorNow() || { status: "?", text: "?" };
      function row(label, value) {
        var g = dlg.add("group"); g.alignChildren = ["left", "top"];
        var l = g.add("statictext", undefined, label); l.preferredSize.width = 80;
        var v = g.add("statictext", undefined, String(value).substr(0, 90), { truncate: "middle" }); v.preferredSize.width = 330;
      }
      row("Status", d.status + " \u2014 " + d.text);
      row("Engine", ENGINE.version + " (" + ENGINE.buildStamp + ")");
      row("Pinned to", d.pin || "(no pin)");
      row("Last heal", UIS.lastHeal || "\u2014");
      var rowB = dlg.add("group"); rowB.alignment = ["fill", "top"];
      var bArch2 = rowB.add("button", undefined, "Archive");
      bArch2.helpTip = "Freeze dependencies, keep working: sidecar + provenance.json. Purely additive.";
      var bPkg2 = rowB.add("button", undefined, "Package for any AE");
      bPkg2.helpTip = "Deliberate exit: effects \u2192 Adobe, sidecar pinned, archive artifacts.";
      var bAdopt2 = rowB.add("button", undefined, "Adopt minColor FX");
      var managed = (d.status !== "unmanaged");
      bArch2.enabled = managed; bPkg2.enabled = managed; bAdopt2.enabled = managed;
      dlg.add("button", undefined, "Close", { name: "ok" });
      bArch2.onClick = function () { dlg.close(); guard("Archive", function () { var r = runCmd("minColor: Archive Project", {}, "archive"); return "sidecar + provenance"; }); };
      bPkg2.onClick = function () { dlg.close(); guard("Package", function () { var r = runCmd("minColor: Package for Any AE", {}, "package"); if (r.failed && r.failed.length) alert("minColor \u2014 package\n\n" + r.failed.join("\n").substr(0, 3000)); return r.translated + " effect(s) \u2192 Adobe, sidecar pinned"; }); };
      bAdopt2.onClick = function () { dlg.close(); guard("Adopt", function () { var r = runCmd("minColor: Adopt Effects", {}, "adopt"); return "adopted " + r.converted + " effect(s)"; }); };
      dlg.show();
    } catch (eP) { alert("minColor: " + eP); }
  };

  // ---- Set Up / Migrate dialog ----
  var pSetup = section("Setup Project", GLYPH.gear);
  var bSetup = flatButton(pSetup, "Set Up / Migrate Project\u2026");
  bSetup.onClick = function () {
    var dlg = new Window("dialog", "minColor \u2014 Set Up / Migrate");
    dlg.orientation = "column"; dlg.alignChildren = ["fill", "top"]; dlg.margins = 14; dlg.spacing = 8;
    var hdr = dlg.add("group"); hdr.spacing = 6; hdr.alignChildren = ["left", "center"]; hdr.alignment = ["fill", "top"];
    var hic = hdr.add("iconbutton", undefined, undefined, { style: "toolbutton" }); hic.preferredSize = [18, 18];
    hic.onDraw = function () { GLYPH.gear(this.graphics); };
    var hst = hdr.add("statictext", undefined, "Set Up / Migrate");
    try { hst.graphics.font = ScriptUI.newFont("dialog", "BOLD", 11); } catch (eHd) {}
    var hln = hdr.add("panel"); hln.alignment = ["fill", "center"]; hln.preferredSize.height = 2; hln.minimumSize.width = 20;
    var r1 = dlg.add("group"); r1.add("statictext", undefined, "Working-space preset:");
    var dd = r1.add("dropdownlist", undefined, []);
    var pj = readJSON(SETTINGS + "/plugin-menus.json");            // labels: presets.json isn't shell-readable; keys come from the engine's presets
    var pcfg = readJSON((($.os.indexOf("Windows") >= 0) ? "C:/Program Files/Adobe/Common/Plug-ins/7.0/MediaCore/minColor" :
                        "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor") + "/configs/presets.json") ||
               readJSON((($.os.indexOf("Windows") >= 0) ? "C:/ProgramData/minColor" : "/Users/Shared/minColor") + "/configs/presets.json");
    var pkeys = [];
    if (pcfg && pcfg.presets) { for (var pk2 in pcfg.presets) pkeys.push(pk2); }
    for (var pi = 0; pi < pkeys.length; pi++) {
      var itP = dd.add("item", (pcfg.presets[pkeys[pi]].label || pkeys[pi]));
      itP.key = pkeys[pi];
    }
    if (dd.items.length) dd.selection = 0;
    bindDD(dd, "setupPreset");
    dlg.add("statictext", undefined, "New Project: seeds a fresh project (asks where to save).");
    dlg.add("statictext", undefined, "Migrate Current: strips footage assignments (harvested as suggestions),");
    dlg.add("statictext", undefined, "sets working space + sidecar config, one save/backup/reopen.");
    var r2 = dlg.add("group"); r2.alignment = ["right", "top"];
    var bNew = flatButton(r2, "New Project", { width: 100 });
    var bMig = flatButton(r2, "Migrate Current", { width: 124, primary: true });
    var bCxl = flatButton(r2, "Cancel", { width: 72, outline: true });
    bCxl.onClick = function () { dlg.close(2); };
    try { dlg.cancelElement = bCxl; } catch (eCE) {}
    var choice = null;
    bNew.onClick = function () { choice = "new"; dlg.close(1); };
    bMig.onClick = function () { choice = "migrate"; dlg.close(1); };
    if (dlg.show() !== 1 || !choice || !dd.selection) return;
    var presetKey = dd.selection.key, presetLabel = dd.selection.text;
    if (choice === "new") {
      guard("New Project", function () {                            // thin composition: new + save + native Set Up
        var f = File.saveDialog("Save the new project", "After Effects:*.aep");
        if (!f) return "cancelled";
        var p = f.fsName; if (!/\.aep$/i.test(p)) p += ".aep";
        app.newProject();
        app.project.save(new File(p));
        var r = runCmd("minColor: Set Up Project", { preset: presetKey }, "setup");
        return "working=" + r.working;
      });
    } else {
      guard("Migrate", function () {
        // no confirm(): clicking "Migrate Current" in the dialog (which explains the op) IS the
        // confirmation, and migrate backs up first — a second yes/no was one popup too many.
        var r = runCmd("minColor: Migrate Project", { preset: presetKey }, "migrate");
        try { runCmd("minColor: Utility Layers", { view: ddView.selection ? ddView.selection.text : null, render: ddRender.selection ? ddRender.selection.text : null }, "utility-layers"); } catch (eU) {}
        try { repopulateMenus(); } catch (eRp2) {}
        var warn = [];
        if (r.effectsFailed && r.effectsFailed.length) warn.push("CST REBUILD FAILED:\n  " + r.effectsFailed.join("\n  "));
        if (r.effectsRemapped && r.effectsRemapped.length) warn.push("REMAPPED to this preset's spaces (review):\n  " + r.effectsRemapped.join("\n  "));
        if (r.effectsRemoved && r.effectsRemoved.length) warn.push("REMOVED \u2014 no equivalent in this preset:\n  " + r.effectsRemoved.join("\n  "));
        if (r.strippedPipeline && r.strippedPipeline.length) warn.push("REMOVED non-minColor OCIO pipeline effects (project is backed up):\n  " + r.strippedPipeline.join("\n  "));
        if (r.gradesLeft && r.gradesLeft.length) warn.push("OCIO CDL/File grades left in place (verify their look):\n  " + r.gradesLeft.join("\n  "));
        if (r.orphanLayers && r.orphanLayers.length) warn.push("EMPTY minColor VIEW/RENDER layers (safe to delete):\n  " + r.orphanLayers.join("\n  "));
        if (warn.length) alert("minColor \u2014 migrate warnings\n\n" + warn.join("\n\n").substr(0, 3000));
        return "working=" + r.working + (r.bitsPerChannel ? " \u00b7 " + r.bitsPerChannel + " bpc" : "") +
               " | pin: " + (r.pinLocus || "?") + " | stripped=" + r.stripped + " rebuilt=" + (r.effectsRebuilt || 0) +
               " remapped=" + (r.effectsRemapped ? r.effectsRemapped.length : 0) +
               " view/render retargeted=" + (r.viewRenderRetargeted || 0) + " residual=" + r.residual +
               " | backups: " + (r.backups ? r.backups.count + " (" + r.backups.mb + " MB)" : "?");
      });
    }
  };

  // ---- Interpret ----
  function interpretDetail(label, r) {
    /* success is silent \u2014 the summary lands on the status line (the return value below) and
       the full breakdown is in reports/interpret*-last.json. Only FAILURES pop a dialog. */
    if (r.failed && r.failed.length) alert("minColor \u2014 " + label + " \u2014 failed\n\n  " + cap(r.failed));
    return "added " + (r.added ? r.added.length : 0) + ", failed " + (r.failed ? r.failed.length : 0) +
           ", identity " + (r.identity ? r.identity.length : 0) + ", skipped " + (r.skipped ? r.skipped.length : 0);
  }
  var pFtg = section("Interpret Footage", GLYPH.film);
  var rowS = hrow(pFtg); rowS.add("statictext", undefined, "Selected as:");
  var ddSrc = rowS.add("dropdownlist", undefined, MENUS.inputSpaces || []); if (ddSrc.items.length) ddSrc.selection = 0;
  ddSrc.alignment = ["fill", "center"]; ddSrc.preferredSize.width = 150;
  var bSel = flatButton(rowS, "Apply", { width: 60 });
  bindDD(ddSrc, "interpretSpace");
  bSel.onClick = function () {
    guard("Interpret selected", function () {
      var r = runCmd("minColor: Interpret Selected", { space: ddSrc.selection ? ddSrc.selection.text : null }, "interpret-selected");
      return interpretDetail("Interpret selected", r);
    });
  };
  var pTL = section("Interpret Timeline", GLYPH.bars);
  var rowA = hrow(pTL);
  var bComp = flatButton(rowA, "Interpret timeline", { primary: true });
  bComp.helpTip = "Active comp + nested precomps, auto-suggested per item; contained precomps are treated as media";
  var bMatches = flatButton(rowA, "Matches\u2026", { width: 90 });
  var rowSt = hrow(pTL);
  var bStrip = flatButton(rowSt, "Strip foreign OCIO", { outline: true });
  var bStripAll = flatButton(rowSt, "Strip ALL", { width: 90, outline: true });
  bComp.onClick = function () {
    guard("Interpret timeline", function () {
      var comp = app.project.activeItem; if (!(comp instanceof CompItem)) throw new Error("open a comp");
      var r = runCmd("minColor: Interpret Timeline", {}, "interpret");
      var s = interpretDetail("Interpret timeline", r);
      var u = runCmd("minColor: Utility Layers", { view: ddView.selection ? ddView.selection.text : null, render: ddRender.selection ? ddRender.selection.text : null }, "utility-layers");
      return s + " | view " + u.view + " (" + u.viewAction + "), render " + u.render + " (" + u.renderAction + ")";
    });
  };
  bStripAll.helpTip = "DEMOLITION: remove EVERY OCIO effect from this timeline + precomps \u2014 foreign AND minColor, file grades included (listed), view/render layers deleted, containment ignored. One undo reverses it.";
  bStripAll.onClick = function () {
    if (!confirm("minColor \u2014 Strip ALL OCIO\n\nRemove EVERY OCIO effect from this timeline and its precomps?\n\n\u2022 foreign AND minColor effects\n\u2022 CDL/FILE grades too (listed in the report)\n\u2022 minColor view/render layers deleted\n\u2022 contained precomps NOT spared\n\nOne undo reverses everything.")) return;
    guard("Strip ALL OCIO", function () {
      var r = runCmd("minColor: Strip ALL", {}, "strip-all");
      if (r.failed.length) alert("minColor \u2014 strip ALL \u2014 failed\n\n  " + cap(r.failed));   /* success is silent */
      return "stripped " + r.stripped.length + ", layers removed " + r.layersRemoved.length;
    });
  };
  bStrip.helpTip = "Remove non-minColor OCIO pipeline effects (Display/Look/CST) from this timeline + precomps. File grades and minColor effects stay; contained precomps are skipped. Undoable.";
  bStrip.onClick = function () {
    guard("Strip foreign OCIO", function () {
      var r = runCmd("minColor: Strip Foreign OCIO", {}, "strip-foreign");
      if (r.failed.length) alert("minColor \u2014 strip \u2014 failed\n\n  " + cap(r.failed));   /* success is silent */
      return "stripped " + r.stripped.length + ", grades left " + r.gradesLeft.length;
    });
  };
  bMatches.onClick = function () {                                  // extension table: pure settings CRUD, shell-owned
    var dlg = new Window("dialog", "minColor \u2014 Extension matches");
    dlg.orientation = "column"; dlg.alignChildren = ["fill", "top"]; dlg.margins = 14; dlg.spacing = 8;
    var mh = dlg.add("group"); mh.spacing = 6; mh.alignChildren = ["left", "center"]; mh.alignment = ["fill", "top"];
    var mic = mh.add("iconbutton", undefined, undefined, { style: "toolbutton" }); mic.preferredSize = [18, 18];
    mic.onDraw = function () { GLYPH.bars(this.graphics); };
    var mst = mh.add("statictext", undefined, "Extension matches");
    try { mst.graphics.font = ScriptUI.newFont("dialog", "BOLD", 11); } catch (eMh) {}
    var mln = mh.add("panel"); mln.alignment = ["fill", "center"]; mln.preferredSize.height = 2; mln.minimumSize.width = 20;
    dlg.add("statictext", undefined, "Extension \u2192 colour space the Interpret passes assume. \"working\" = leave as is.");
    var lb = dlg.add("listbox", undefined, [], { numberOfColumns: 2, showHeaders: true, columnTitles: ["ext", "space"], columnWidths: [70, 200] });
    lb.preferredSize = [300, 180];
    function extMap() { var j = readJSON(SETTINGS + "/extension-defaults.json"); return (j && j.defaults) ? j.defaults : {}; }
    function saveExtMap(m) {
      var parts = [], k, ks = []; for (k in m) ks.push(k); ks.sort();
      for (var i = 0; i < ks.length; i++) parts.push(jstr(ks[i]) + ": " + jstr(m[ks[i]]));
      writeText(SETTINGS + "/extension-defaults.json", '{ "defaults": { ' + parts.join(", ") + " } }\n");
    }
    function refreshMap() {
      lb.removeAll();
      var m = extMap(), ks = [], k; for (k in m) ks.push(k); ks.sort();
      for (var i = 0; i < ks.length; i++) { var it = lb.add("item", ks[i]); it.subItems[0].text = m[ks[i]]; }
    }
    var row = dlg.add("group");
    var et = row.add("edittext", undefined, ""); et.characters = 6;
    var dds = row.add("dropdownlist", undefined, ["working (identity)"].concat(MENUS.inputSpaces || [])); dds.selection = 0; dds.preferredSize.width = 200;
    bindDD(dds, "matchesSpace");
    var row2 = dlg.add("group"); row2.alignment = ["right", "top"];
    var bSet = flatButton(row2, "Add / Update", { width: 104, primary: true });
    var bDel = flatButton(row2, "Remove", { width: 76, outline: true });
    var bDone = flatButton(row2, "Done", { width: 64 });
    try { dlg.defaultElement = bDone; } catch (eDe) {}
    lb.onChange = function () { if (lb.selection) { et.text = lb.selection.text; var want = lb.selection.subItems[0].text; dds.selection = 0; for (var i = 1; i < dds.items.length; i++) if (dds.items[i].text === want) dds.selection = i; } };
    function applyRule(quiet) {
      try { var ext = et.text.replace(/^\./, "").toLowerCase(); if (!ext) { if (quiet) return; throw new Error("type an extension"); }
        var m = extMap(); m[ext] = (dds.selection.index === 0) ? "working" : dds.selection.text;
        saveExtMap(m); refreshMap(); } catch (e) { alert(String(e)); }
    }
    bSet.onClick = function () { applyRule(false); };
    bDone.onClick = function () {
      try {
        var ext = et.text.replace(/^\./, "").toLowerCase();
        if (ext) { var m = extMap(); var want = (dds.selection.index === 0) ? "working" : dds.selection.text;
          if (m[ext] !== want) applyRule(true); }
      } catch (eD) {}
      dlg.close(1);
    };
    bDel.onClick = function () { try { if (!lb.selection) throw new Error("select a row"); var m = extMap(); delete m[lb.selection.text]; saveExtMap(m); refreshMap(); } catch (e) { alert(String(e)); } };
    refreshMap(); dlg.show();
  };

  // ---- Adjustment Layer ----
  var pAdj = section("Adjustment Layer", GLYPH.adj);
  var rowV = hrow(pAdj); rowV.add("statictext", undefined, "View:");
  var ddView = rowV.add("dropdownlist", undefined, MENUS.viewSpaces || []); if (ddView.items.length) ddView.selection = 0;
  ddView.alignment = ["fill", "center"]; ddView.preferredSize.width = 150;
  bindDD(ddView, "viewSpace");
  var bView = flatButton(rowV, "Apply", { width: 80, primary: true });
  bView.helpTip = "Apply this VIEW to the comp's guide layer \u2014 viewport only, never renders";
  bView.onClick = function () {
    guard("View guide", function () {
      var r = runCmd("minColor: Utility Layers", { view: ddView.selection ? ddView.selection.text : null }, "utility-layers");
      return r.view + " (" + r.viewAction + ")";
    });
  };
  var rowR = hrow(pAdj); rowR.add("statictext", undefined, "Render:");
  var ddRender = rowR.add("dropdownlist", undefined, MENUS.renderSpaces || []);
  ddRender.alignment = ["fill", "center"]; ddRender.preferredSize.width = 150;
  if (ddRender.items.length) ddRender.selection = 0;
  if (MENUS.defaultRender) for (var rdi = 0; rdi < ddRender.items.length; rdi++) if (ddRender.items[rdi].text === MENUS.defaultRender) { ddRender.selection = rdi; break; }
  bindDD(ddRender, "renderSpace");
  var bRender = flatButton(rowR, "Apply", { width: 80, primary: true });
  bRender.helpTip = "Apply this RENDER to the comp's render layer (NOT a guide \u2014 it renders): working space \u2192 delivery space";
  bRender.onClick = function () {
    guard("Render layer", function () {
      var r = runCmd("minColor: Utility Layers", { render: ddRender.selection ? ddRender.selection.text : null }, "utility-layers");
      return r.render + " (" + r.renderAction + ")";
    });
  };
  var rowL = hrow(pAdj); rowL.add("statictext", undefined, "Look:");
  var ddLook = rowL.add("dropdownlist", undefined, ["(none)"].concat(MENUS.looks || [])); ddLook.selection = 0;
  ddLook.alignment = ["fill", "center"]; ddLook.preferredSize.width = 150;
  bindDD(ddLook, "lookChoice");
  rowL.enabled = (MENUS.looks || []).length > 0;
  var bLook = flatButton(rowL, "Apply", { width: 80 });
  bLook.helpTip = "Set or remove the look on the existing view/render layers (applies before the display transform; spaces untouched)";
  bLook.onClick = function () {
    guard("Look", function () {
      var lk = (ddLook.selection.index === 0) ? "" : ddLook.selection.text;
      var r = runCmd("minColor: Apply Look", { look: lk }, "apply-look");
      return (lk || "none") + " (view " + r.view + ", render " + r.render + ")";
    });
  };
  var rowP = hrow(pAdj); rowP.add("statictext", undefined, "Preset:");
  var ddPreset = rowP.add("dropdownlist", undefined, renderPresetNames());
  if (ddPreset.items.length) ddPreset.selection = 0;
  ddPreset.alignment = ["fill", "center"]; ddPreset.preferredSize.width = 150;
  bindDD(ddPreset, "renderPreset");
  var bPreset = flatButton(rowP, "Apply", { width: 80 });
  bPreset.helpTip = "Apply a render preset recipe: rewrites BOTH view and render layers (look + spaces); view stays enabled";
  rowP.visible = ddPreset.items.length > 0;                          // hidden when no recipes (parity with the panel)
  bPreset.onClick = function () {
    guard("Render preset", function () {
      var r = runCmd("minColor: Apply Render Preset", { name: ddPreset.selection.text }, "render-preset");
      return r.preset + " (view " + r.view + ", render " + r.render + ", look " + r.look + ")";
    });
  };

  // ---- per-preset menus: dropdowns follow plugin-menus.json (the engine follows the pin) ----
  function refill(dd, items, key, preferred) {
    var cur = dd.selection ? dd.selection.text : null, onCh = dd.onChange; dd.onChange = null;
    dd.removeAll(); for (var i = 0; i < items.length; i++) dd.add("item", items[i]);
    var want = [cur, UIS[key], preferred], sel = -1;
    for (var w = 0; w < want.length && sel < 0; w++) if (want[w]) for (var j = 0; j < dd.items.length; j++) if (dd.items[j].text === want[w]) { sel = j; break; }
    if (dd.items.length) dd.selection = (sel < 0 ? 0 : sel);
    if (dd.selection) dd.helpTip = dd.selection.text;
    dd.onChange = onCh;                                              // UIS is NOT written here: only user changes persist
  }
  function repopulateMenus() {
    if (!loadMenus()) return;
    refill(ddSrc, MENUS.inputSpaces || [], "interpretSpace", null);
    refill(ddView, MENUS.viewSpaces || [], "viewSpace", MENUS.defaultView);
    refill(ddRender, MENUS.renderSpaces || [], "renderSpace", MENUS.defaultRender);
    refill(ddLook, ["(none)"].concat(MENUS.looks || []), "lookChoice", null);
    rowL.enabled = (MENUS.looks || []).length > 0;
    var rps = renderPresetNames();
    refill(ddPreset, rps, "renderPreset", null);
    rowP.visible = rps.length > 0;
    fitRows();
  }

  // ---- footer: the ENGINE is the version now ----
  var rowF = win.add("group");
  var status = rowF.add("statictext", undefined, "ready \u00b7 engine " + (ENGINE.version || "?"), { truncate: "end" });
  status.alignment = ["fill", "center"];
  status.helpTip = "minColor engine " + (ENGINE.version || "?") + " \u00b7 " + (ENGINE.buildStamp || "?");

  docText.text = "checking\u2026";
  /* NO refreshDoctor here: a docked panel initializes during AE STARTUP, and an
     executeCommand fired there wedges the launch (spin of record, 2026-09-01). The first
     check rides the heartbeat below; the lamp shows "checking..." for its first 5 s.     */
  try { if ($.global.__minColorTask) app.cancelTask($.global.__minColorTask); } catch (eC) {}
  $.global.__minColorTick = function () { try { if (win.visible) refreshDoctor(true); } catch (eK) {} };
  try { $.global.__minColorTask = app.scheduleTask("if ($.global.__minColorTick) $.global.__minColorTick();", 5000, true); } catch (eS) {}
  win.layout.layout(true); fitRows();
  win.onResizing = win.onResize = function () { this.layout.resize(); fitRows(); };
  if (win instanceof Window) { win.center(); win.show(); }
 } catch (eTop) {
  try { var elog = new File(Folder.temp.fsName + "/minColor_shell_error.txt"); elog.open("w");
        elog.write(eTop.toString() + " line " + eTop.line); elog.close(); } catch (e2) {}
 }
})(this);
