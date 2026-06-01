#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"$ROOT_DIR/scripts/cmake_build.sh"
CTEST_PARALLEL_LEVEL="${CTEST_PARALLEL_LEVEL:-1}" \
    ctest --test-dir "$ROOT_DIR/build/cmake" --output-on-failure -j "${CTEST_PARALLEL_LEVEL:-1}"
