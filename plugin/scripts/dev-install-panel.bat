@echo off
rem Install the 2.0 shell into the newest AE version's per-user ScriptUI Panels folder (no elevation).
rem lean-v3: ONE shell (minColor.jsx), NO minColor-data payload (retired). Purges the old 0.9.x panel
rem so AE stops loading it. Fails loudly if build.py dies - a dead build must never leave the old
rem panel silently in place.
setlocal
set "REPO=%~dp0..\.."
set "AEROOT=%APPDATA%\Adobe\After Effects"
set "VER="
for /f "delims=" %%v in ('dir /b /ad /o:n "%AEROOT%\2*" 2^>nul') do set "VER=%%v"
if not defined VER echo no After Effects preference folder under %AEROOT% - launch AE once first & exit /b 1
set "DEST=%AEROOT%\%VER%\Scripts\ScriptUI Panels"
python "%REPO%\build\build.py" || (echo build.py FAILED - panel NOT installed & exit /b 1)
if not exist "%REPO%\dist-panel\minColor.jsx" echo dist-panel\minColor.jsx missing - panel NOT installed & exit /b 1
if not exist "%DEST%" mkdir "%DEST%"
rem retire the 0.9.x payload so AE loads only the 2.0 shell (the shell reads NO minColor-data; the
rem AEGP seeds settings\ from its embedded copies at launch)
if exist "%DEST%\minColor-data" rmdir /s /q "%DEST%\minColor-data"
if exist "%DEST%\minColor Panel.jsx" del /q "%DEST%\minColor Panel.jsx"
copy /y "%REPO%\dist-panel\minColor.jsx" "%DEST%\minColor.jsx" >nul || exit /b 1
echo installed panel -^> %DEST%  (AE %VER%; restart AE)
echo NOTE: if an OLD 0.9.2 was installed by an MSI (per-machine), uninstall it or remove
echo   "Program Files\Adobe\Adobe After Effects %VER%\Support Files\Scripts\ScriptUI Panels\minColor*.jsx"
exit /b 0
