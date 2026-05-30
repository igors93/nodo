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

cmake -S "$ROOT_DIR" -B "$CMAKE_BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$CMAKE_BUILD_DIR" --parallel
ctest --test-dir "$CMAKE_BUILD_DIR" --output-on-failure

echo
echo "CMake tests completed successfully."
