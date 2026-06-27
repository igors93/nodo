#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

: "${NODO_BUILD_JOBS:=1}"
export NODO_BUILD_JOBS
: "${CTEST_PARALLEL_LEVEL:=1}"
export CTEST_PARALLEL_LEVEL

"$ROOT_DIR/scripts/cmake_build.sh"

ctest --test-dir "$ROOT_DIR/build/cmake" \
    -R "(p2p_EncryptedPeerChannelTests|p2p_EncryptedPeerTransportTests|p2p_EncryptedPeerHandshakeTests)" \
    --output-on-failure \
    -j "$CTEST_PARALLEL_LEVEL"
