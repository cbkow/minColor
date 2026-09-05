@echo off
rem Install the Windows build (WINDOWS.md). Counterpart of dev-install-both.sh:
rem   effect  minColorCST.aex (+ version.txt) -> MediaCore\minColor
rem   AEGP    minColorAEGP.aex                 -> each installed AE's Support Files\Plug-ins
rem lean-v3 is STORE-LESS: the effect embeds its configs+LUTs, the AEGP embeds its metadata and
rem seeds ProgramData\minColor\settings on launch - NO configs folder is installed.
rem MediaCore + AE Plug-ins live under Program Files, so this self-elevates (one UAC prompt) if needed.
setlocal
set "HERE=%~dp0.."
set "DEST=%ProgramFiles%\Adobe\Common\Plug-ins\7.0\MediaCore\minColor"
set "PLUGIN=%HERE%\build\Release\minColorCST.aex"
set "AEGP=%HERE%\build\Release\minColorAEGP.aex"
set "VERSION=%HERE%\build\Release\version.txt"
if not exist "%PLUGIN%" echo build first: cmake --build plugin\build --config Release & exit /b 1
if not exist "%AEGP%" echo minColorAEGP.aex missing - build the AEGP target (WINDOWS.md section 2) & exit /b 1
if not exist "%VERSION%" echo version.txt missing beside the .aex - rebuild with the current CMakeLists & exit /b 1
if not exist "%DEST%" mkdir "%DEST%" 2>nul
copy /y "%PLUGIN%" "%DEST%\minColorCST.aex" >nul 2>nul && goto :have_access
if "%~1"=="/elevated" echo copy failed even when elevated & exit /b 1
echo MediaCore is not writable from this prompt - requesting elevation...
powershell -NoProfile -Command "Start-Process -FilePath cmd.exe -ArgumentList '/c','\"%~f0\" /elevated' -Verb RunAs -Wait" || exit /b 1
if not exist "%DEST%\version.txt" exit /b 1
goto :done
:have_access
copy /y "%VERSION%" "%DEST%\version.txt" >nul || exit /b 1
rem AEGP -> every installed AE's app Plug-ins (ceremonies + the handshake the panel gates on)
set "AEGPHIT="
for /d %%A in ("%ProgramFiles%\Adobe\Adobe After Effects 20*") do (
  if exist "%%A\Support Files\Plug-ins" copy /y "%AEGP%" "%%A\Support Files\Plug-ins\minColorAEGP.aex" >nul && set "AEGPHIT=1"
)
if not defined AEGPHIT echo WARNING: no AE Support Files\Plug-ins found - AEGP NOT installed (panel stays gated)
:done
echo installed -^> %DEST%  (minColorCST.aex, version.txt) + minColorAEGP.aex into AE Plug-ins  - restart AE
exit /b 0
