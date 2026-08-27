#!/bin/bash
# Build the standalone engine probe (no AE SDK needed). Run external/build.sh first.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"; I="$HERE/external/install"
DEPS=""
for lib in expat Imath-3_2 minizip-ng pystring yaml-cpp z; do
  DEPS="$DEPS $HERE/external/build/ext/dist/lib/lib$lib.a"
done
clang++ -std=c++17 -O2 -arch arm64 -I "$I/include" \
  "$HERE/tools/probe-engine.cpp" "$HERE/src/OcioEngine.cpp" \
  "$I/lib/libOpenColorIO.a" $DEPS \
  -framework CoreFoundation -framework ColorSync -framework CoreGraphics -framework IOKit \
  -o "$HERE/tools/probe-engine"
echo "built tools/probe-engine"
