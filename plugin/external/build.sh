#!/bin/bash
# Build a pinned, static, universal (arm64+x86_64) OpenColorIO for the minColor plugin.
# Produces external/install/{lib,include}. Idempotent; delete external/{src,build,install} to redo.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
TAG="v2.5.2"                                # pin policy per QCView dependencies.md; bump deliberately
SRC="$HERE/src/OpenColorIO"; BUILD="$HERE/build"; INSTALL="$HERE/install"
if [ ! -d "$SRC" ]; then
  mkdir -p "$HERE/src"
  git clone --depth 1 --branch "$TAG" https://github.com/AcademySoftwareFoundation/OpenColorIO.git "$SRC"
fi
cmake -S "$SRC" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$INSTALL" \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_CXX_VISIBILITY_PRESET=hidden \
  -DCMAKE_VISIBILITY_INLINES_HIDDEN=ON \
  -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_TESTS=OFF -DOCIO_BUILD_GPU_TESTS=OFF \
  -DOCIO_BUILD_PYTHON=OFF -DOCIO_BUILD_DOCS=OFF \
  -DOCIO_INSTALL_EXT_PACKAGES=ALL
cmake --build "$BUILD" --target install -j "$(sysctl -n hw.ncpu)"
echo "== done =="; ls "$INSTALL/lib" | head
