#!/usr/bin/env bash
set -euo pipefail

# Nodo crypto test script.
# This script builds and runs cryptographic hash and signature provider tests.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests"

mkdir -p "$BUILD_DIR"

echo "Building Nodo C crypto module for crypto tests..."

gcc -std=c11 -Wall -Wextra -I"$ROOT_DIR/include" \
    -c "$ROOT_DIR/src/crypto/hash.c" \
    -o "$BUILD_DIR/hash_crypto_test.o"

echo "Building Nodo crypto hash tests..."

g++ -std=c++20 -Wall -Wextra -I"$ROOT_DIR/include" \
    "$ROOT_DIR/tests/crypto/HashTests.cpp" \
    "$BUILD_DIR/hash_crypto_test.o" \
    -o "$BUILD_DIR/crypto_hash_tests"

echo "Building Nodo signature provider tests..."

g++ -std=c++20 -Wall -Wextra -I"$ROOT_DIR/include" \
    "$ROOT_DIR/tests/crypto/SignatureProviderTests.cpp" \
    "$ROOT_DIR/src/crypto/CryptoAlgorithm.cpp" \
    "$ROOT_DIR/src/crypto/CryptoPolicy.cpp" \
    "$ROOT_DIR/src/crypto/PublicKey.cpp" \
    "$ROOT_DIR/src/crypto/PrivateKey.cpp" \
    "$ROOT_DIR/src/crypto/Signature.cpp" \
    "$ROOT_DIR/src/crypto/DevelopmentSignatureProvider.cpp" \
    "$ROOT_DIR/src/crypto/SignatureBundle.cpp" \
    "$BUILD_DIR/hash_crypto_test.o" \
    -o "$BUILD_DIR/signature_provider_tests"

echo
echo "Running Nodo crypto hash tests..."
"$BUILD_DIR/crypto_hash_tests"

echo
echo "Running Nodo signature provider tests..."
"$BUILD_DIR/signature_provider_tests"

echo
echo "Crypto tests completed successfully."
