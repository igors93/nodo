#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests"

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

echo "Building Nodo C crypto module for storage integration tests..."
"$CC" "${CFLAGS[@]}" \
    -c "$ROOT_DIR/src/crypto/hash.c" \
    -o "$BUILD_DIR/hash_storage_test.o"

echo "Building Nodo blockchain storage integration tests..."
"$CXX" "${CXXFLAGS[@]}" \
    "$ROOT_DIR/tests/storage/BlockchainStorageIntegrationTests.cpp" \
    "$ROOT_DIR/src/utils/Amount.cpp" \
    "$ROOT_DIR/src/utils/Time.cpp" \
    "$ROOT_DIR/src/economics/MintRecord.cpp" \
    "$ROOT_DIR/src/serialization/FieldCodec.cpp" \
    "$ROOT_DIR/src/serialization/MintRecordCodec.cpp" \
    "$ROOT_DIR/src/serialization/PrivacyCommitmentCodec.cpp" \
    "$ROOT_DIR/src/serialization/PrivacyNullifierCodec.cpp" \
    "$ROOT_DIR/src/serialization/PrivateAccountingRecordCodec.cpp" \
    "$ROOT_DIR/src/serialization/ChainManifestCodec.cpp" \
    "$ROOT_DIR/src/serialization/BlockStorageIndexCodec.cpp" \
    "$ROOT_DIR/src/serialization/BlockSnapshotHeaderCodec.cpp" \
    "$ROOT_DIR/src/serialization/LedgerRecordCodec.cpp" \
    "$ROOT_DIR/src/serialization/BlockCodec.cpp" \
    "$ROOT_DIR/src/storage/BlockFileStore.cpp" \
    "$ROOT_DIR/src/storage/ChainManifest.cpp" \
    "$ROOT_DIR/src/storage/BlockStorageIndex.cpp" \
    "$ROOT_DIR/src/storage/BlockchainStorageReader.cpp" \
    "$ROOT_DIR/src/storage/BlockchainLoader.cpp" \
    "$ROOT_DIR/src/storage/BlockSnapshotHeader.cpp" \
    "$ROOT_DIR/src/privacy/PrivacyCommitment.cpp" \
    "$ROOT_DIR/src/privacy/PrivacyNullifier.cpp" \
    "$ROOT_DIR/src/privacy/NullifierSet.cpp" \
    "$ROOT_DIR/src/privacy/PrivateAccountingRecord.cpp" \
    "$ROOT_DIR/src/privacy/PrivateAccountingLedger.cpp" \
    "$ROOT_DIR/src/privacy/PrivateAccountingLedgerRebuilder.cpp" \
    "$ROOT_DIR/src/core/Account.cpp" \
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
    "$ROOT_DIR/src/crypto/DevelopmentSignatureProvider.cpp" \
    "$ROOT_DIR/src/crypto/SignatureBundle.cpp" \
    "$ROOT_DIR/src/crypto/Address.cpp" \
    "$ROOT_DIR/src/crypto/AddressDerivation.cpp" \
    "$ROOT_DIR/src/crypto/KeyPair.cpp" \
    "$ROOT_DIR/src/crypto/PostQuantumAlgorithmProfile.cpp" \
    "$ROOT_DIR/src/crypto/PostQuantumMigrationPlan.cpp" \
    "$BUILD_DIR/hash_storage_test.o" \
    -o "$BUILD_DIR/blockchain_storage_integration_tests"

echo
echo "Running Nodo blockchain storage integration tests..."
"$BUILD_DIR/blockchain_storage_integration_tests"

echo
echo "Blockchain storage integration tests completed successfully."
