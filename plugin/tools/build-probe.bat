@echo off
rem Build the standalone engine probe with MSVC (no AE SDK needed). Windows counterpart of build-probe.sh.
rem Run from an x64 Native Tools prompt after external\build.bat.
setlocal
set "HERE=%~dp0.."
set "I=%HERE%\external\install"
set "E=%HERE%\external\build\ext\dist\lib"
rem OpenColorIO_SKIP_IMPORTS: static OCIO (CMake gets it from the imported target); user32/gdi32: OCIO's monitor enumeration
cl /nologo /std:c++17 /O2 /EHsc /MD /DOpenColorIO_SKIP_IMPORTS /I "%I%\include" ^
  "%HERE%\tools\probe-engine.cpp" "%HERE%\src\OcioEngine.cpp" ^
  /Fe:"%HERE%\tools\probe-engine.exe" /Fo"%TEMP%\\" ^
  /link "%I%\lib\OpenColorIO.lib" "%E%\Imath-3_2.lib" "%E%\libexpatMD.lib" "%E%\minizip-ng.lib" ^
        "%E%\pystring.lib" "%E%\yaml-cpp.lib" "%E%\zlibstatic.lib" user32.lib gdi32.lib || exit /b 1
echo built tools\probe-engine.exe
