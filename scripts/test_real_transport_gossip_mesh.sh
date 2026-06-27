#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

: "${NODO_BUILD_JOBS:=1}"
: "${CTEST_PARALLEL_LEVEL:=1}"

"$ROOT_DIR/scripts/cmake_build.sh"

ctest --test-dir "$ROOT_DIR/build/cmake" \
    --output-on-failure \
    -j "$CTEST_PARALLEL_LEVEL" \
    -R "p2p_(LoopbackTransportTests|TransportFrameCodecTests|GossipMeshTests|GossipMeshNetworkRejectionTests|PeerHandshakeManagerTests)"
