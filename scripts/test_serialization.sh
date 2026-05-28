#!/usr/bin/env bash
set -euo pipefail

# Nodo serialization test script.
# This script builds and runs serialization round-trip tests.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests"

mkdir -p "$BUILD_DIR"

echo "Building Nodo C crypto module for tests..."

gcc -std=c11 -Wall -Wextra -I"$ROOT_DIR/include" \
    -c "$ROOT_DIR/src/crypto/hash.c" \
    -o "$BUILD_DIR/hash_test.o"

echo "Building Nodo serialization round-trip tests..."

g++ -std=c++20 -Wall -Wextra -I"$ROOT_DIR/include" \
    "$ROOT_DIR/tests/serialization/SerializationRoundTripTests.cpp" \
    "$ROOT_DIR/src/utils/Amount.cpp" \
    "$ROOT_DIR/src/economics/MintRecord.cpp" \
    "$ROOT_DIR/src/serialization/FieldCodec.cpp" \
    "$ROOT_DIR/src/serialization/MintRecordCodec.cpp" \
    "$ROOT_DIR/src/serialization/PrivacyCommitmentCodec.cpp" \
    "$ROOT_DIR/src/serialization/PrivacyNullifierCodec.cpp" \
    "$ROOT_DIR/src/serialization/PrivateAccountingRecordCodec.cpp" \
    "$ROOT_DIR/src/privacy/PrivacyCommitment.cpp" \
    "$ROOT_DIR/src/privacy/PrivacyNullifier.cpp" \
    "$ROOT_DIR/src/privacy/PrivateAccountingRecord.cpp" \
    "$BUILD_DIR/hash_test.o" \
    -o "$BUILD_DIR/serialization_roundtrip_tests"

echo
echo "Running Nodo serialization round-trip tests..."
"$BUILD_DIR/serialization_roundtrip_tests"

echo
echo "Serialization round-trip tests completed successfully."
