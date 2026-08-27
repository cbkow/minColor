@echo off
rem Install the Windows build into MediaCore (WINDOWS.md step 5). Counterpart of dev-install.sh.
rem   minColorCST.aex + version.txt (panel's pluginBundleVersion contract) + config\dist as "configs"
rem   (the central config store; without it the panel silently pins sidecars next to projects).
rem MediaCore lives under Program Files, so this self-elevates (one UAC prompt) when needed.
setlocal
set "HERE=%~dp0.."
set "DEST=%ProgramFiles%\Adobe\Common\Plug-ins\7.0\MediaCore\minColor"
set "PLUGIN=%HERE%\build\Release\minColorCST.aex"
set "VERSION=%HERE%\build\Release\version.txt"
set "CONFIGS=%HERE%\..\config\dist"
if not exist "%PLUGIN%" echo build first: cmake --build plugin\build --config Release & exit /b 1
if not exist "%VERSION%" echo version.txt missing beside the .aex - rebuild with the current CMakeLists & exit /b 1
if not exist "%CONFIGS%\presets.json" echo config\dist not generated (config\generate.py) & exit /b 1
if not exist "%DEST%" mkdir "%DEST%" 2>nul
copy /y "%PLUGIN%" "%DEST%\minColorCST.aex" >nul 2>nul && goto :have_access
if "%~1"=="/elevated" echo copy failed even when elevated & exit /b 1
echo MediaCore is not writable from this prompt - requesting elevation...
powershell -NoProfile -Command "Start-Process -FilePath cmd.exe -ArgumentList '/c','\"%~f0\" /elevated' -Verb RunAs -Wait" || exit /b 1
if not exist "%DEST%\version.txt" exit /b 1
goto :done
:have_access
copy /y "%VERSION%" "%DEST%\version.txt" >nul || exit /b 1
xcopy /e /y /i /q "%CONFIGS%" "%DEST%\configs" >nul || exit /b 1
:done
echo installed -^> %DEST%  (minColorCST.aex, version.txt, configs\)  - restart AE
exit /b 0
