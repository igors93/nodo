#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

: "${NODO_BUILD_JOBS:=1}"
export NODO_BUILD_JOBS
: "${CTEST_PARALLEL_LEVEL:=1}"
export CTEST_PARALLEL_LEVEL

"$ROOT_DIR/scripts/cmake_build.sh"

ctest --test-dir "$ROOT_DIR/build/cmake" \
    -R "(consensus_test_slashing_double_vote_evidence|consensus_test_evidence_pool|consensus_test_validator_accountability|storage_test_slashing_evidence_store|node_test_slashing_evidence_messages)" \
    --output-on-failure \
    -j "$CTEST_PARALLEL_LEVEL"
