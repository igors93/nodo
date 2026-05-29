#!/usr/bin/env bash
set -euo pipefail

# Nodo protection economics test script.
# Important linker rule:
# Each .cpp implementation file must appear only once per g++ command.

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

echo "Building Nodo protection ledger integration tests..."

g++ -std=c++20 -Wall -Wextra -I"$ROOT_DIR/include" \
    "$ROOT_DIR/tests/economics/ProtectionLedgerIntegrationTests.cpp" \
    "$ROOT_DIR/src/utils/Amount.cpp" \
    "$ROOT_DIR/src/utils/Time.cpp" \
    "$ROOT_DIR/src/economics/MintRecord.cpp" \
    "$ROOT_DIR/src/serialization/MintRecordCodec.cpp" \
    "$ROOT_DIR/src/serialization/FieldCodec.cpp" \
    "$ROOT_DIR/src/economics/ValidationWorkRecord.cpp" \
    "$ROOT_DIR/src/economics/ValidatorScoreRecord.cpp" \
    "$ROOT_DIR/src/economics/EpochEmissionPolicy.cpp" \
    "$ROOT_DIR/src/economics/ProtectionEpoch.cpp" \
    "$ROOT_DIR/src/economics/GenesisRewardRecord.cpp" \
    "$ROOT_DIR/src/serialization/LedgerRecordCodec.cpp" \
    "$ROOT_DIR/src/privacy/PrivacyCommitment.cpp" \
    "$ROOT_DIR/src/privacy/PrivacyNullifier.cpp" \
    "$ROOT_DIR/src/privacy/PrivateAccountingRecord.cpp" \
    "$ROOT_DIR/src/core/CoinLot.cpp" \
    "$ROOT_DIR/src/core/Account.cpp" \
    "$ROOT_DIR/src/core/Transaction.cpp" \
    "$ROOT_DIR/src/core/LedgerRecord.cpp" \
    "$ROOT_DIR/src/core/Block.cpp" \
    "$ROOT_DIR/src/crypto/CryptoAlgorithm.cpp" \
    "$ROOT_DIR/src/crypto/CryptoPolicy.cpp" \
    "$ROOT_DIR/src/crypto/PublicKey.cpp" \
    "$ROOT_DIR/src/crypto/PrivateKey.cpp" \
    "$ROOT_DIR/src/crypto/Signature.cpp" \
    "$ROOT_DIR/src/crypto/DevelopmentSignatureProvider.cpp" \
    "$ROOT_DIR/src/crypto/SignatureBundle.cpp" \
    "$BUILD_DIR/hash_economics_test.o" \
    -o "$BUILD_DIR/protection_ledger_integration_tests"

echo "Building Nodo protection state rebuilder tests..."

g++ -std=c++20 -Wall -Wextra -I"$ROOT_DIR/include" \
    "$ROOT_DIR/tests/economics/ProtectionStateRebuilderTests.cpp" \
    "$ROOT_DIR/src/utils/Amount.cpp" \
    "$ROOT_DIR/src/utils/Time.cpp" \
    "$ROOT_DIR/src/economics/MintRecord.cpp" \
    "$ROOT_DIR/src/serialization/MintRecordCodec.cpp" \
    "$ROOT_DIR/src/serialization/FieldCodec.cpp" \
    "$ROOT_DIR/src/economics/ValidationWorkRecord.cpp" \
    "$ROOT_DIR/src/economics/ValidatorScoreRecord.cpp" \
    "$ROOT_DIR/src/economics/EpochEmissionPolicy.cpp" \
    "$ROOT_DIR/src/economics/ProtectionEpoch.cpp" \
    "$ROOT_DIR/src/economics/GenesisRewardRecord.cpp" \
    "$ROOT_DIR/src/economics/ProtectionEconomicsState.cpp" \
    "$ROOT_DIR/src/economics/ProtectionEconomicsRebuilder.cpp" \
    "$ROOT_DIR/src/serialization/LedgerRecordCodec.cpp" \
    "$ROOT_DIR/src/privacy/PrivacyCommitment.cpp" \
    "$ROOT_DIR/src/privacy/PrivacyNullifier.cpp" \
    "$ROOT_DIR/src/privacy/PrivateAccountingRecord.cpp" \
    "$ROOT_DIR/src/core/CoinLot.cpp" \
    "$ROOT_DIR/src/core/Account.cpp" \
    "$ROOT_DIR/src/core/Transaction.cpp" \
    "$ROOT_DIR/src/core/LedgerRecord.cpp" \
    "$ROOT_DIR/src/core/Block.cpp" \
    "$ROOT_DIR/src/core/Blockchain.cpp" \
    "$ROOT_DIR/src/crypto/CryptoAlgorithm.cpp" \
    "$ROOT_DIR/src/crypto/CryptoPolicy.cpp" \
    "$ROOT_DIR/src/crypto/PublicKey.cpp" \
    "$ROOT_DIR/src/crypto/PrivateKey.cpp" \
    "$ROOT_DIR/src/crypto/Signature.cpp" \
    "$ROOT_DIR/src/crypto/DevelopmentSignatureProvider.cpp" \
    "$ROOT_DIR/src/crypto/SignatureBundle.cpp" \
    "$BUILD_DIR/hash_economics_test.o" \
    -o "$BUILD_DIR/protection_state_rebuilder_tests"

echo "Building Nodo coin lot registry tests..."

g++ -std=c++20 -Wall -Wextra -I"$ROOT_DIR/include" \
    "$ROOT_DIR/tests/core/CoinLotRegistryTests.cpp" \
    "$ROOT_DIR/src/utils/Amount.cpp" \
    "$ROOT_DIR/src/utils/Time.cpp" \
    "$ROOT_DIR/src/economics/MintRecord.cpp" \
    "$ROOT_DIR/src/serialization/MintRecordCodec.cpp" \
    "$ROOT_DIR/src/serialization/FieldCodec.cpp" \
    "$ROOT_DIR/src/economics/ValidationWorkRecord.cpp" \
    "$ROOT_DIR/src/economics/ValidatorScoreRecord.cpp" \
    "$ROOT_DIR/src/economics/EpochEmissionPolicy.cpp" \
    "$ROOT_DIR/src/economics/ProtectionEpoch.cpp" \
    "$ROOT_DIR/src/economics/GenesisRewardRecord.cpp" \
    "$ROOT_DIR/src/economics/ProtectionEconomicsState.cpp" \
    "$ROOT_DIR/src/economics/ProtectionEconomicsRebuilder.cpp" \
    "$ROOT_DIR/src/serialization/LedgerRecordCodec.cpp" \
    "$ROOT_DIR/src/privacy/PrivacyCommitment.cpp" \
    "$ROOT_DIR/src/privacy/PrivacyNullifier.cpp" \
    "$ROOT_DIR/src/privacy/PrivateAccountingRecord.cpp" \
    "$ROOT_DIR/src/core/CoinLot.cpp" \
    "$ROOT_DIR/src/core/CoinLotVerificationResult.cpp" \
    "$ROOT_DIR/src/core/CoinLotRegistry.cpp" \
    "$ROOT_DIR/src/core/CoinLotRegistryRebuilder.cpp" \
    "$ROOT_DIR/src/core/Account.cpp" \
    "$ROOT_DIR/src/core/Transaction.cpp" \
    "$ROOT_DIR/src/core/LedgerRecord.cpp" \
    "$ROOT_DIR/src/core/Block.cpp" \
    "$ROOT_DIR/src/core/Blockchain.cpp" \
    "$ROOT_DIR/src/crypto/CryptoAlgorithm.cpp" \
    "$ROOT_DIR/src/crypto/CryptoPolicy.cpp" \
    "$ROOT_DIR/src/crypto/PublicKey.cpp" \
    "$ROOT_DIR/src/crypto/PrivateKey.cpp" \
    "$ROOT_DIR/src/crypto/Signature.cpp" \
    "$ROOT_DIR/src/crypto/DevelopmentSignatureProvider.cpp" \
    "$ROOT_DIR/src/crypto/SignatureBundle.cpp" \
    "$BUILD_DIR/hash_economics_test.o" \
    -o "$BUILD_DIR/coin_lot_registry_tests"

echo "Building Nodo coin lot transaction integration tests..."

g++ -std=c++20 -Wall -Wextra -I"$ROOT_DIR/include" \
    "$ROOT_DIR/tests/core/CoinLotTransactionIntegrationTests.cpp" \
    "$ROOT_DIR/src/utils/Amount.cpp" \
    "$ROOT_DIR/src/utils/Time.cpp" \
    "$ROOT_DIR/src/economics/MintRecord.cpp" \
    "$ROOT_DIR/src/serialization/MintRecordCodec.cpp" \
    "$ROOT_DIR/src/serialization/FieldCodec.cpp" \
    "$ROOT_DIR/src/core/Account.cpp" \
    "$ROOT_DIR/src/core/CoinLot.cpp" \
    "$ROOT_DIR/src/core/CoinLotVerificationResult.cpp" \
    "$ROOT_DIR/src/core/CoinLotRegistry.cpp" \
    "$ROOT_DIR/src/core/CoinLotTransactionValidationResult.cpp" \
    "$ROOT_DIR/src/core/CoinLotTransferPlan.cpp" \
    "$ROOT_DIR/src/core/CoinLotTransactionValidator.cpp" \
    "$ROOT_DIR/src/core/State.cpp" \
    "$ROOT_DIR/src/core/Transaction.cpp" \
    "$ROOT_DIR/src/staking/SecurityWeight.cpp" \
    "$ROOT_DIR/src/crypto/CryptoAlgorithm.cpp" \
    "$ROOT_DIR/src/crypto/CryptoPolicy.cpp" \
    "$ROOT_DIR/src/crypto/PublicKey.cpp" \
    "$ROOT_DIR/src/crypto/PrivateKey.cpp" \
    "$ROOT_DIR/src/crypto/Signature.cpp" \
    "$ROOT_DIR/src/crypto/DevelopmentSignatureProvider.cpp" \
    "$ROOT_DIR/src/crypto/SignatureBundle.cpp" \
    "$BUILD_DIR/hash_economics_test.o" \
    -o "$BUILD_DIR/coin_lot_transaction_integration_tests"


echo "Building Nodo explicit transaction input tests..."

g++ -std=c++20 -Wall -Wextra -I"$ROOT_DIR/include" \
    "$ROOT_DIR/tests/core/TransactionExplicitInputTests.cpp" \
    "$ROOT_DIR/src/utils/Amount.cpp" \
    "$ROOT_DIR/src/utils/Time.cpp" \
    "$ROOT_DIR/src/economics/MintRecord.cpp" \
    "$ROOT_DIR/src/serialization/MintRecordCodec.cpp" \
    "$ROOT_DIR/src/serialization/FieldCodec.cpp" \
    "$ROOT_DIR/src/core/Account.cpp" \
    "$ROOT_DIR/src/core/CoinLot.cpp" \
    "$ROOT_DIR/src/core/CoinLotVerificationResult.cpp" \
    "$ROOT_DIR/src/core/CoinLotRegistry.cpp" \
    "$ROOT_DIR/src/core/CoinLotTransactionValidationResult.cpp" \
    "$ROOT_DIR/src/core/CoinLotTransferPlan.cpp" \
    "$ROOT_DIR/src/core/CoinLotTransactionValidator.cpp" \
    "$ROOT_DIR/src/core/State.cpp" \
    "$ROOT_DIR/src/core/Transaction.cpp" \
    "$ROOT_DIR/src/staking/SecurityWeight.cpp" \
    "$ROOT_DIR/src/crypto/CryptoAlgorithm.cpp" \
    "$ROOT_DIR/src/crypto/CryptoPolicy.cpp" \
    "$ROOT_DIR/src/crypto/PublicKey.cpp" \
    "$ROOT_DIR/src/crypto/PrivateKey.cpp" \
    "$ROOT_DIR/src/crypto/Signature.cpp" \
    "$ROOT_DIR/src/crypto/DevelopmentSignatureProvider.cpp" \
    "$ROOT_DIR/src/crypto/SignatureBundle.cpp" \
    "$BUILD_DIR/hash_economics_test.o" \
    -o "$BUILD_DIR/transaction_explicit_input_tests"

echo
echo "Running Nodo protection economics tests..."
"$BUILD_DIR/protection_economics_tests"

echo
echo "Running Nodo protection ledger integration tests..."
"$BUILD_DIR/protection_ledger_integration_tests"

echo
echo "Running Nodo protection state rebuilder tests..."
"$BUILD_DIR/protection_state_rebuilder_tests"

echo
echo "Running Nodo coin lot registry tests..."
"$BUILD_DIR/coin_lot_registry_tests"

echo
echo "Running Nodo coin lot transaction integration tests..."
"$BUILD_DIR/coin_lot_transaction_integration_tests"


echo
echo "Running Nodo explicit transaction input tests..."
"$BUILD_DIR/transaction_explicit_input_tests"

echo
echo "Protection economics tests completed successfully."
