#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_BUILD_DIR="$ROOT_DIR/build/cmake"

if ! command -v cmake >/dev/null 2>&1; then
    echo "Error: cmake was not found in PATH."
    echo "Fedora: sudo dnf install cmake"
    echo "MSYS2 UCRT64: pacman -S mingw-w64-ucrt-x86_64-cmake"
    exit 1
fi

: "${CTEST_PARALLEL_LEVEL:=1}"
export CTEST_PARALLEL_LEVEL

"$ROOT_DIR/scripts/cmake_build.sh"
ctest --test-dir "$CMAKE_BUILD_DIR" --output-on-failure -j "$CTEST_PARALLEL_LEVEL"

echo
echo "CMake tests completed successfully."
