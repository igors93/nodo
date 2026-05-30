#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_BUILD_DIR="$ROOT_DIR/build/cmake"

if ! command -v cmake >/dev/null 2>&1; then
    echo "Error: cmake was not found in PATH."
    echo "Fedora: sudo dnf install cmake"
    exit 1
fi

"$ROOT_DIR/scripts/cmake_build.sh"
ctest --test-dir "$CMAKE_BUILD_DIR" --output-on-failure -L crypto

echo
echo "Crypto CMake tests completed successfully."
