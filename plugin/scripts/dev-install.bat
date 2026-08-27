@echo off
rem Copy the built .aex into MediaCore for iteration. Windows counterpart of dev-install.sh.
rem MediaCore lives under Program Files, so this self-elevates (one UAC prompt) when needed.
setlocal
set "HERE=%~dp0.."
set "DEST=%ProgramFiles%\Adobe\Common\Plug-ins\7.0\MediaCore\minColor"
set "PLUGIN=%HERE%\build\Release\minColorCST.aex"
if not exist "%PLUGIN%" echo build first: cmake --build plugin\build --config Release & exit /b 1
if not exist "%DEST%" mkdir "%DEST%" 2>nul
copy /y "%PLUGIN%" "%DEST%\minColorCST.aex" >nul 2>nul && goto :done
if "%~1"=="/elevated" echo copy failed even when elevated & exit /b 1
echo MediaCore is not writable from this prompt - requesting elevation...
powershell -NoProfile -Command "Start-Process -FilePath cmd.exe -ArgumentList '/c','\"%~f0\" /elevated' -Verb RunAs -Wait" || exit /b 1
if not exist "%DEST%\minColorCST.aex" exit /b 1
:done
echo installed -^> %DEST%\minColorCST.aex (restart AE)
exit /b 0
