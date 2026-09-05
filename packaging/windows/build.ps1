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
# WiX harvests each source path once, so trees delivered to two places get their own staged copy (mirrored, so stale files drop out).
$Stage = "$Root\dist-panel\msi-stage"
function Mirror($From, $To) { robocopy $From $To /MIR /NFL /NDL /NJH /NJS /NP | Out-Null; if ($LASTEXITCODE -ge 8) { throw "robocopy $From -> $To failed ($LASTEXITCODE)" } }
Mirror "$Root\config\dist" "$Stage\configs-central"
Mirror "$Root\config\dist" "$Stage\configs-shared"
foreach ($ae in "2025", "2026") {
  # lean-v3: ONE shell for both platforms (the 0.9.2 windows-panel + minColor-data are retired).
  Copy-Item "$Root\dist-panel\minColor.jsx" "$Stage\panel-$ae\minColor.jsx" -Force
}
# AEGP .aex (ceremonies) — staged for the installer's AEGP component (see minColor.wxs). Built by
# the Windows CMake WIN32 minColorAEGP target and committed to plugin\prebuilt\windows.
if (Test-Path "$Root\plugin\prebuilt\windows\minColorAEGP.aex") {
  Copy-Item "$Root\plugin\prebuilt\windows\minColorAEGP.aex" "$Stage\minColorAEGP.aex" -Force
} else { Write-Warning "minColorAEGP.aex not in plugin\prebuilt\windows — the .msi will install the effect + panel but NO ceremonies, and the panel will stay gated. Build the WIN32 AEGP target first (plugin\WINDOWS.md §3)." }
$Out = "$Root\dist-panel\minColor-$Ver.msi"
wix build "$PSScriptRoot\minColor.wxs" -ext WixToolset.Util.wixext -arch x64 `
  -d "Version=$Ver" -d "EngineVersion=$EngineVer" -d "Prebuilt=$Root\plugin\prebuilt\windows" -d "Config=$Root\config" `
  -d "ConfigsCentral=$Stage\configs-central" -d "ConfigsShared=$Stage\configs-shared" `
  -d "Panel2025=$Stage\panel-2025" -d "Panel2026=$Stage\panel-2026" -o $Out
if ($LASTEXITCODE) { throw "wix build failed ($LASTEXITCODE)" }
Write-Host "-> $Out"
Write-Host "silent install:  msiexec /i `"$Out`" /qn      uninstall: msiexec /x `"$Out`" /qn"
Write-Host "(every build gets a new ProductCode; to uninstall a build whose .msi you no longer have: msiexec /x <ProductCode> - the {GUID} key under HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall whose DisplayName is minColor)"
