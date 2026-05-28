#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

CC="${CC:-gcc}"
CXX="${CXX:-g++}"

CFLAGS=(
    -std=c11
    -Wall
    -Wextra
    -I"$ROOT_DIR/include"
)

CXXFLAGS=(
    -std=c++20
    -Wall
    -Wextra
    -I"$ROOT_DIR/include"
)

mkdir -p "$BUILD_DIR"

echo "Building Nodo C crypto module..."
"$CC" "${CFLAGS[@]}" \
    -c "$ROOT_DIR/src/crypto/hash.c" \
    -o "$BUILD_DIR/hash.o"

echo "Building Nodo C++ application..."
"$CXX" "${CXXFLAGS[@]}" \
    "$ROOT_DIR/apps/cli/main.cpp" \
    "$ROOT_DIR/src/app/DemoScenario.cpp" \
    "$ROOT_DIR/src/utils/Amount.cpp" \
    "$ROOT_DIR/src/utils/Time.cpp" \
    "$ROOT_DIR/src/economics/MintRecord.cpp" \
    "$ROOT_DIR/src/core/CoinLot.cpp" \
    "$ROOT_DIR/src/core/State.cpp" \
    "$ROOT_DIR/src/core/Transaction.cpp" \
    "$ROOT_DIR/src/core/LedgerRecord.cpp" \
    "$ROOT_DIR/src/core/Block.cpp" \
    "$ROOT_DIR/src/core/Blockchain.cpp" \
    "$ROOT_DIR/src/core/ChainStateRebuilder.cpp" \
    "$ROOT_DIR/src/staking/SecurityWeight.cpp" \
    "$ROOT_DIR/src/crypto/CryptoAlgorithm.cpp" \
    "$ROOT_DIR/src/crypto/CryptoPolicy.cpp" \
    "$ROOT_DIR/src/crypto/PublicKey.cpp" \
    "$ROOT_DIR/src/crypto/PrivateKey.cpp" \
    "$ROOT_DIR/src/crypto/Signature.cpp" \
    "$ROOT_DIR/src/crypto/SignatureBundle.cpp" \
    "$BUILD_DIR/hash.o" \
    -o "$BUILD_DIR/nodo"

echo "Build completed successfully."
echo "Executable: $BUILD_DIR/nodo"