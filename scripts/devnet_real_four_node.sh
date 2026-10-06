#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${NODO_CMAKE_DIR:-$ROOT_DIR/build/cmake}"

if [ ! -f "$BUILD_DIR/CTestTestfile.cmake" ]; then
    echo "Configure and build Nodo before running the four-validator devnet." >&2
    exit 2
fi

# The test launches four independent validator processes, joins them over
# authenticated loopback TCP, finalizes a block and audits every chain.
ctest --test-dir "$BUILD_DIR" -R '^node_FourValidatorDevnetTests$' \
    --output-on-failure --parallel 1
