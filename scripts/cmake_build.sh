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

cmake_args=(
    -DCMAKE_BUILD_TYPE=Debug
)

if command -v make >/dev/null 2>&1; then
    cmake_args+=(
        -G "Unix Makefiles"
        -DCMAKE_MAKE_PROGRAM="$(command -v make)"
    )
fi

cmake -S "$ROOT_DIR" -B "$CMAKE_BUILD_DIR" "${cmake_args[@]}"
cmake --build "$CMAKE_BUILD_DIR" --parallel "${NODO_BUILD_JOBS:-1}"

echo
echo "CMake build completed successfully."
echo "Executable:"
echo "  $ROOT_DIR/build/nodo"
