#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

: "${NODO_BUILD_JOBS:=1}"
export NODO_BUILD_JOBS
: "${CTEST_PARALLEL_LEVEL:=1}"
export CTEST_PARALLEL_LEVEL

"$ROOT_DIR/scripts/cmake_build.sh"

ctest --test-dir "$ROOT_DIR/build/cmake" \
    -R "(consensus_test_validator_penalty_policy|consensus_test_validator_penalty_application|storage_test_validator_penalty_store|node_test_validator_penalty_messages)" \
    --output-on-failure \
    -j "$CTEST_PARALLEL_LEVEL"
