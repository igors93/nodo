#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_BUILD_DIR="$ROOT_DIR/build/cmake"

if ! command -v cmake >/dev/null 2>&1; then
    echo "Error: cmake was not found in PATH."
    exit 1
fi

cmake -S "$ROOT_DIR" -B "$CMAKE_BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$CMAKE_BUILD_DIR" --parallel
ctest --test-dir "$CMAKE_BUILD_DIR" --output-on-failure -R "ForkChoiceTests|PeerMessageTests|LocalNodeSyncTests"

echo
echo "Cycle 5 CMake tests completed successfully."
