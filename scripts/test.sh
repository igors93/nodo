#!/usr/bin/env bash
# Nodo test runner.
#
# Usage:
#   scripts/test.sh                Build and run the full test suite.
#   scripts/test.sh <module>       Build and run one module's tests, where
#                                  <module> is a tests/ subdirectory used as
#                                  the ctest name prefix: app, config,
#                                  consensus, core, crypto, economics,
#                                  mempool, node, p2p, serialization,
#                                  staking, storage, utils.
#   scripts/test.sh -R <regex>     Build and run tests whose ctest names
#                                  match <regex>.
#
# CTEST_PARALLEL_LEVEL and NODO_BUILD_JOBS are honoured (both default to 1).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_BUILD_DIR="$ROOT_DIR/build/cmake"

: "${CTEST_PARALLEL_LEVEL:=1}"
export CTEST_PARALLEL_LEVEL

"$ROOT_DIR/scripts/cmake_build.sh"

ctest_args=(
    --test-dir "$CMAKE_BUILD_DIR"
    --output-on-failure
    --no-tests=error
    -j "$CTEST_PARALLEL_LEVEL"
)

if [ "$#" -eq 0 ]; then
    :
elif [ "$1" = "-R" ]; then
    if [ "$#" -lt 2 ]; then
        echo "Usage: scripts/test.sh -R <regex>" >&2
        exit 1
    fi
    ctest_args+=(-R "$2")
else
    ctest_args+=(-R "^$1_")
fi

ctest "${ctest_args[@]}"

echo
echo "Nodo tests completed successfully."
