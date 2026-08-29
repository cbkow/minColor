# minColor Windows installer build (run on the Windows box after: cmake build -> prebuilt refreshed -> python build\build.py)
#   powershell -ExecutionPolicy Bypass -File packaging\windows\build.ps1
# Output: dist-panel\minColor-<ver>.msi (unsigned; SmartScreen shows "unknown publisher" once per machine — Authenticode later signs the same MSI)
$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Ver  = (Select-String -Path "$Root\src\minColor.jsxinc" -Pattern 'var VERSION = "([^"]+)"').Matches[0].Groups[1].Value
if (-not (Get-Command wix -ErrorAction SilentlyContinue)) { dotnet tool install --global wix }
wix extension add -g WixToolset.Util.wixext | Out-Null
$Out = "$Root\dist-panel\minColor-$Ver.msi"
wix build "$PSScriptRoot\minColor.wxs" -ext WixToolset.Util.wixext -arch x64 `
  -d "Version=$Ver" -d "Prebuilt=$Root\plugin\prebuilt\windows" -d "Configs=$Root\config\dist" `
  -d "Config=$Root\config" -d "Panel=$Root\dist-panel" -o $Out
Write-Host "-> $Out"
Write-Host "silent install:  msiexec /i `"$Out`" /qn      uninstall: msiexec /x `"$Out`" /qn"
