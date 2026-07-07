#include "core/TransactionBuilder.hpp"
#include "core/TransactionPayload.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/Bls12381SignatureProvider.hpp"
#include "crypto/Ed25519SignatureProvider.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/Signer.hpp"
#include "node/StakingRegistry.hpp"
#include "utils/Amount.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::core::Transaction;
using nodo::core::TransactionBuilder;
using nodo::core::TransactionBuildRequest;
using nodo::core::TransactionType;
using nodo::core::ValidatorKeyRotationPayload;
using nodo::core::ValidatorRegistrationRecord;
using nodo::core::ValidatorRegistry;
using nodo::core::ValidatorRegistryUpdateStatus;
using nodo::crypto::AddressDerivation;
using nodo::crypto::Bls12381SignatureProvider;
using nodo::crypto::Ed25519SignatureProvider;
using nodo::crypto::KeyPair;
using nodo::crypto::Signer;
using nodo::node::StakingRegistry;
using nodo::utils::Amount;

constexpr std::int64_t kNow = 1900000000;

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

ValidatorRegistrationRecord recordFor(const KeyPair &key,
                                      std::uint64_t activationEpoch,
                                      const std::string &metadataHash,
                                      std::int64_t timestamp) {
  return ValidatorRegistrationRecord(
      AddressDerivation::deriveFromPublicKey(key.publicKey()).value(),
      key.publicKey(), activationEpoch, metadataHash, timestamp);
}

void testValidatorKeyRotationPayloadRoundTrip() {
  const KeyPair oldKey =
      KeyPair::createDeterministicBls12381KeyPair("old-validator");
  const KeyPair newKey =
      KeyPair::createDeterministicBls12381KeyPair("new-validator");
  const std::string oldAddress =
      AddressDerivation::deriveFromPublicKey(oldKey.publicKey()).value();

  const ValidatorKeyRotationPayload payload(oldAddress, newKey.publicKey(),
                                            "rotation-metadata", 7,
                                            "operator-approved-rotation");

  require(payload.isValid(), "Rotation payload should be valid.");
  require(
      payload.newValidatorAddress() ==
          AddressDerivation::deriveFromPublicKey(newKey.publicKey()).value(),
      "Rotation payload should derive the new validator address.");

  const ValidatorKeyRotationPayload decoded =
      ValidatorKeyRotationPayload::deserialize(payload.serialize());
  require(decoded.serialize() == payload.serialize(),
          "Rotation payload should serialize canonically.");
}

void testValidatorRegistryRotatesKeyAndPreservesOwnerStakeAndStatus() {
  const KeyPair oldKey =
      KeyPair::createDeterministicBls12381KeyPair("registry-old");
  const KeyPair newKey =
      KeyPair::createDeterministicBls12381KeyPair("registry-new");
  const std::string owner = "owner-address";

  ValidatorRegistry registry;
  const auto registered = registry.registerValidator(
      recordFor(oldKey, 1, "old-metadata", kNow),
      ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS, owner);
  require(registered.accepted(), "Original validator should register.");

  const auto rotated = registry.rotateValidatorKey(
      registered.entry().registrationRecord().validatorAddress(),
      recordFor(newKey, 2, "new-metadata", kNow + 10), kNow + 10);

  require(rotated.status() == ValidatorRegistryUpdateStatus::KEY_ROTATED,
          "Registry should report key rotation.");
  require(!registry.hasValidator(
              registered.entry().registrationRecord().validatorAddress()),
          "Old validator address should leave the active registry map.");
  require(registry.hasValidator(
              rotated.entry().registrationRecord().validatorAddress()),
          "New validator address should enter the registry map.");
  require(rotated.entry().ownerAddress() == owner,
          "Rotation must preserve owner address.");
  require(rotated.entry().stakeAmount() ==
              ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS,
          "Rotation must preserve validator stake.");
  require(registry.isValid(), "Registry should remain valid after rotation.");
}

void testStakingRegistryRotatesValidatorAddress() {
  StakingRegistry staking;
  staking.deposit("owner-a", "validator-old", Amount::fromRawUnits(2'000'000),
                  10, false, "tx-deposit");
  staking.activatePending(11);

  staking.rotateValidatorAddress("validator-old", "validator-new", "owner-a",
                                 12, "tx-rotate");

  require(!staking.hasAccount("validator-old"),
          "Old validator stake account should be removed.");
  require(staking.hasAccount("validator-new"),
          "New validator stake account should exist.");
  require(staking.activeStake("owner-a", "validator-new").rawUnits() ==
              2'000'000,
          "Owner active stake should move to the new validator address.");
  require(staking.activeStake("owner-a", "validator-old").isZero(),
          "Old validator address should no longer own active stake.");
  require(staking.isValid(),
          "Staking registry should remain valid after rotation.");
}

void testBuilderCreatesSignedValidatorKeyRotationTransaction() {
  const KeyPair ownerKey =
      KeyPair::createDeterministicEd25519KeyPair("owner-key");
  const KeyPair oldValidatorKey =
      KeyPair::createDeterministicBls12381KeyPair("builder-old-validator");
  const KeyPair newValidatorKey =
      KeyPair::createDeterministicBls12381KeyPair("builder-new-validator");
  const Ed25519SignatureProvider provider;
  const Signer signer(ownerKey, provider);
  const std::string oldAddress =
      AddressDerivation::deriveFromPublicKey(oldValidatorKey.publicKey())
          .value();

  const Transaction tx = TransactionBuilder::buildSignedValidatorKeyRotation(
      TransactionBuildRequest(oldAddress, Amount(), Amount::fromRawUnits(100),
                              1, kNow),
      newValidatorKey.publicKey(), "rotation-metadata", 4, "operator-approved",
      signer, "chain-test");

  require(tx.type() == TransactionType::VALIDATOR_KEY_ROTATE,
          "Transaction type should be VALIDATOR_KEY_ROTATE.");
  require(tx.toAddress() == oldAddress,
          "Rotation transaction target should be old validator address.");
  const ValidatorKeyRotationPayload payload =
      ValidatorKeyRotationPayload::deserialize(tx.data());
  require(
      payload.newValidatorAddress() ==
          AddressDerivation::deriveFromPublicKey(newValidatorKey.publicKey())
              .value(),
      "Signed transaction should carry the new validator public key.");
  require(tx.hasSignatureBundle(),
          "Rotation transaction should be signed by owner key.");
}

} // namespace

int main() {
  try {
    testValidatorKeyRotationPayloadRoundTrip();
    testValidatorRegistryRotatesKeyAndPreservesOwnerStakeAndStatus();
    testStakingRegistryRotatesValidatorAddress();
    testBuilderCreatesSignedValidatorKeyRotationTransaction();
    std::cout << "Nodo validator key rotation tests passed.\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Nodo validator key rotation tests failed: " << e.what()
              << "\n";
    return 1;
  }
}
