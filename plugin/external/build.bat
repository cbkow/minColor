@echo off
rem Static OpenColorIO 2.5.2 for the minColor plugin (Windows / Visual Studio).
rem Run from a "x64 Native Tools" VS command prompt. Produces external\install.
setlocal
set TAG=v2.5.2
set HERE=%~dp0
if not exist "%HERE%src\OpenColorIO" (
  mkdir "%HERE%src" 2>nul
  git clone --depth 1 --branch %TAG% https://github.com/AcademySoftwareFoundation/OpenColorIO.git "%HERE%src\OpenColorIO"
)
cmake -S "%HERE%src\OpenColorIO" -B "%HERE%build" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_INSTALL_PREFIX="%HERE%install" ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON ^
  -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_TESTS=OFF -DOCIO_BUILD_GPU_TESTS=OFF ^
  -DOCIO_BUILD_PYTHON=OFF -DOCIO_BUILD_DOCS=OFF ^
  -DOCIO_INSTALL_EXT_PACKAGES=ALL
cmake --build "%HERE%build" --config Release --target install
echo == done ==
dir "%HERE%install\lib"
endlocal
