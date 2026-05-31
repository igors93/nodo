#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_BUILD_DIR="$ROOT_DIR/build/cmake-sanitized"

if ! command -v cmake >/dev/null 2>&1; then
    echo "Error: cmake was not found in PATH."
    exit 1
fi

if ! command -v make >/dev/null 2>&1; then
    echo "Error: make was not found in PATH."
    exit 1
fi

cmake -S "$ROOT_DIR" -B "$CMAKE_BUILD_DIR" \
    -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_MAKE_PROGRAM="$(command -v make)" \
    -DCMAKE_C_FLAGS_DEBUG="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
    -DCMAKE_CXX_FLAGS_DEBUG="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
    -DCMAKE_EXE_LINKER_FLAGS_DEBUG="-fsanitize=address,undefined"

cmake --build "$CMAKE_BUILD_DIR" --parallel

echo
echo "Sanitized CMake build completed successfully."
