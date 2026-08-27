@echo off
rem Build a pinned, static OpenColorIO (x64, Release, /MD) for the minColor plugin on Windows.
rem Windows counterpart of build.sh. Produces external\install\{lib,include} plus the vendored
rem ext deps in external\build\ext\dist\lib. Idempotent; delete external\{src,build,install} to redo.
rem Run from an "x64 Native Tools Command Prompt for VS 2022" (it will locate vcvars64 itself otherwise).
setlocal
set "HERE=%~dp0"
set "HERE=%HERE:~0,-1%"
set "TAG=v2.5.2"
set "SRC=%HERE%\src\OpenColorIO"
set "BUILD=%HERE%\build"
set "INSTALL=%HERE%\install"

where cl >nul 2>nul
if not errorlevel 1 goto :have_cl
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH echo Visual Studio with the C++ x64 toolset not found & exit /b 1
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
:have_cl
rem VS-bundled CMake + Ninja (if a standalone cmake is not already on PATH)
where cmake >nul 2>nul || set "PATH=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
where ninja >nul 2>nul || set "PATH=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

if exist "%SRC%\CMakeLists.txt" goto :have_src
if not exist "%HERE%\src" mkdir "%HERE%\src"
git clone --depth 1 --branch %TAG% https://github.com/AcademySoftwareFoundation/OpenColorIO.git "%SRC%" || exit /b 1
:have_src
cmake -S "%SRC%" -B "%BUILD%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_INSTALL_PREFIX="%INSTALL%" ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL ^
  -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_TESTS=OFF -DOCIO_BUILD_GPU_TESTS=OFF ^
  -DOCIO_BUILD_PYTHON=OFF -DOCIO_BUILD_DOCS=OFF ^
  -DOCIO_INSTALL_EXT_PACKAGES=ALL || exit /b 1
cmake --build "%BUILD%" --target install || exit /b 1
echo == done ==
dir /b "%INSTALL%\lib"
