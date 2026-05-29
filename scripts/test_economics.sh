#!/usr/bin/env bash
set -euo pipefail

# Nodo protection economics test script.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests"

mkdir -p "$BUILD_DIR"

echo "Building Nodo C crypto module for protection economics tests..."

gcc -std=c11 -Wall -Wextra -I"$ROOT_DIR/include" \
    -c "$ROOT_DIR/src/crypto/hash.c" \
    -o "$BUILD_DIR/hash_economics_test.o"

echo "Building Nodo protection economics tests..."

g++ -std=c++20 -Wall -Wextra -I"$ROOT_DIR/include" \
    "$ROOT_DIR/tests/economics/ProtectionEconomicsTests.cpp" \
    "$ROOT_DIR/src/utils/Amount.cpp" \
    "$ROOT_DIR/src/core/CoinLot.cpp" \
    "$ROOT_DIR/src/economics/ValidationWorkRecord.cpp" \
    "$ROOT_DIR/src/economics/ValidatorScoreRecord.cpp" \
    "$ROOT_DIR/src/economics/EpochEmissionPolicy.cpp" \
    "$ROOT_DIR/src/economics/ProtectionEpoch.cpp" \
    "$ROOT_DIR/src/economics/GenesisRewardRecord.cpp" \
    "$BUILD_DIR/hash_economics_test.o" \
    -o "$BUILD_DIR/protection_economics_tests"

echo
echo "Running Nodo protection economics tests..."
"$BUILD_DIR/protection_economics_tests"

echo
echo "Protection economics tests completed successfully."
