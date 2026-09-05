# minColor Windows installer build (run on the Windows box after: cmake build -> prebuilt refreshed -> python build\build.py)
#   powershell -ExecutionPolicy Bypass -File packaging\windows\build.ps1
# Output: dist-panel\minColor-<ver>.msi (unsigned; SmartScreen shows "unknown publisher" once per machine — Authenticode later signs the same MSI)
$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Ver  = (Select-String -Path "$Root\plugin\CMakeLists.txt" -Pattern 'set\(MINC_VERSION "([^"]+)"\)').Matches[0].Groups[1].Value
$EngineVer = (Get-Content "$Root\plugin\prebuilt\windows\version.txt" -Raw).Trim()   # File/@DefaultVersion for the unversioned .aex
# WiX 5.x, pinned: v7 (the unpinned default since 2026) refuses to run until its Open Source Maintenance Fee EULA is accepted,
# and the .wxs is written against the v4/v5 schema. The util extension must match the tool's major.minor.
if (-not (Get-Command wix -ErrorAction SilentlyContinue)) {
  dotnet tool install --global wix --version "5.*"; if ($LASTEXITCODE) { throw "dotnet tool install wix failed" }
}
$WixVer = (& wix --version) -replace '\+.*$', ''
if (-not $WixVer.StartsWith("5.")) { throw "wix $WixVer found; this build needs WiX 5.x (dotnet tool uninstall -g wix; rerun)" }
if (-not ((& wix extension list -g) -match 'WixToolset\.Util\.wixext')) {
  wix extension add -g "WixToolset.Util.wixext/$WixVer"; if ($LASTEXITCODE) { throw "wix extension add failed" }
}
# The MSI ships NO config store and NO settings seed — the effect embeds its configs+LUTs and the
# AEGP embeds the metadata, seeding ProgramData\minColor\settings on first launch (see minColor.wxs).
$Stage = "$Root\dist-panel\msi-stage"
# AEGP .aex (ceremonies) — built by the Windows CMake WIN32 minColorAEGP target and committed to
# plugin\prebuilt\windows. Staged per-AE beside the shell (distinct staged paths, same rule as the
# config trees) for the installer's per-AE Plug-ins components (see minColor.wxs).
$Aegp = "$Root\plugin\prebuilt\windows\minColorAEGP.aex"
if (-not (Test-Path $Aegp)) { throw "minColorAEGP.aex not in plugin\prebuilt\windows: without it the panel installs but stays gated (no ceremonies, no aegp-api.json handshake). Build the WIN32 AEGP target first (plugin\WINDOWS.md section 3). [ASCII only in this file: PS 5.1 reads it ANSI, so an em dash mojibakes into a string-closing smart quote]" }
foreach ($ae in "2025", "2026") {
  # lean-v3: ONE shell for both platforms (the 0.9.2 windows-panel + minColor-data are retired).
  New-Item -ItemType Directory -Force "$Stage\panel-$ae" | Out-Null   # build.py wipes dist-panel, so the stage starts empty
  Copy-Item "$Root\dist-panel\minColor.jsx" "$Stage\panel-$ae\minColor.jsx" -Force
  Copy-Item $Aegp "$Stage\panel-$ae\minColorAEGP.aex" -Force
}
$Out = "$Root\dist-panel\minColor-$Ver.msi"
wix build "$PSScriptRoot\minColor.wxs" -ext WixToolset.Util.wixext -arch x64 `
  -d "Version=$Ver" -d "EngineVersion=$EngineVer" -d "Prebuilt=$Root\plugin\prebuilt\windows" `
  -d "Panel2025=$Stage\panel-2025" -d "Panel2026=$Stage\panel-2026" -o $Out
if ($LASTEXITCODE) { throw "wix build failed ($LASTEXITCODE)" }
Write-Host "-> $Out"
Write-Host "silent install:  msiexec /i `"$Out`" /qn      uninstall: msiexec /x `"$Out`" /qn"
Write-Host "(every build gets a new ProductCode; to uninstall a build whose .msi you no longer have: msiexec /x <ProductCode> - the {GUID} key under HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall whose DisplayName is minColor)"
