@echo off
rem Build the panel distributable and copy it into the newest AE version's per-user ScriptUI Panels
rem folder (no elevation needed). Sibling of dev-install.bat, which covers only the plugin side.
rem Fails loudly if build.py dies — a dead build must never leave the old panel silently in place.
setlocal
set "REPO=%~dp0..\.."
set "AEROOT=%APPDATA%\Adobe\After Effects"
set "VER="
for /f "delims=" %%v in ('dir /b /ad /o:n "%AEROOT%\2*" 2^>nul') do set "VER=%%v"
if not defined VER echo no After Effects preference folder under %AEROOT% - launch AE once first & exit /b 1
set "DEST=%AEROOT%\%VER%\Scripts\ScriptUI Panels"
python "%REPO%\build\build.py" || (echo build.py FAILED - panel NOT installed & exit /b 1)
if not exist "%REPO%\dist-panel\minColor-data\configs\presets.json" echo dist-panel incomplete - panel NOT installed & exit /b 1
if not exist "%DEST%" mkdir "%DEST%"
copy /y "%REPO%\dist-panel\minColor.jsx" "%DEST%\minColor.jsx" >nul || exit /b 1
if exist "%DEST%\minColor-data" rmdir /s /q "%DEST%\minColor-data"
xcopy /e /y /i /q "%REPO%\dist-panel\minColor-data" "%DEST%\minColor-data" >nul || exit /b 1
echo installed panel -^> %DEST%  (AE %VER%; restart AE)
exit /b 0
