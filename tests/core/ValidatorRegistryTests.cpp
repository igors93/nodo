#include "core/ValidatorRegistry.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "crypto/Address.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PublicKey.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using nodo::core::ValidatorRegistrationRecord;
using nodo::core::ValidatorRegistry;
using nodo::core::ValidatorRegistryUpdateStatus;
using nodo::core::ValidatorSetHistory;
using nodo::crypto::Address;
using nodo::crypto::AddressDerivation;
using nodo::crypto::KeyPair;
using nodo::crypto::PublicKey;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(
    bool condition,
    const std::string& failureMessage
) {
    if (!condition) {
        throw std::runtime_error(failureMessage);
    }
}

PublicKey publicKey(
    const std::string& suffix
) {
    return KeyPair::createDeterministicBls12381KeyPair(
        "validator-registry-key-" + suffix
    ).publicKey();
}

ValidatorRegistrationRecord registrationFor(
    const PublicKey& key,
    std::uint64_t activationEpoch,
    const std::string& metadataHash,
    std::int64_t timestamp
) {
    const Address address =
        AddressDerivation::deriveFromPublicKey(key);

    return ValidatorRegistrationRecord(
        address.value(),
        key,
        activationEpoch,
        metadataHash,
        timestamp
    );
}

void testRegistrationBindsAddressToPublicKey() {
    const PublicKey key =
        publicKey("a");

    const ValidatorRegistrationRecord record =
        registrationFor(
            key,
            1,
            "metadata-hash-a",
            kTimestamp
        );

    requireCondition(
        record.isValid(),
        "Validator registration record should be valid."
    );

    requireCondition(
        record.validatorAddress() ==
            AddressDerivation::deriveFromPublicKey(key).value(),
        "Validator address must be derived from public key."
    );

    requireCondition(
        !record.deterministicId().empty(),
        "Valid validator registration record should have deterministic id."
    );
}

void testRegistryAcceptsAndVerifiesValidatorIdentity() {
    const PublicKey key =
        publicKey("b");

    const ValidatorRegistrationRecord record =
        registrationFor(
            key,
            2,
            "metadata-hash-b",
            kTimestamp + 10
        );

    ValidatorRegistry registry;

    const auto result =
        registry.registerValidator(record);

    requireCondition(
        result.accepted(),
        "Valid validator registration should be accepted."
    );

    requireCondition(
        registry.size() == 1U,
        "Registry size should be one after accepted registration."
    );

    requireCondition(
        registry.activeCount() == 1U,
        "Registry active count should be one after accepted registration."
    );

    requireCondition(
        registry.verifyValidatorIdentity(
            record.validatorAddress(),
            key
        ),
        "Registry should verify registered validator identity."
    );

    requireCondition(
        !registry.verifyValidatorIdentity(
            record.validatorAddress(),
            publicKey("wrong")
        ),
        "Registry should reject wrong public key for registered address."
    );
}

void testDuplicateRegistrationIsSafeNoOp() {
    const PublicKey key =
        publicKey("c");

    const ValidatorRegistrationRecord record =
        registrationFor(
            key,
            3,
            "metadata-hash-c",
            kTimestamp + 20
        );

    ValidatorRegistry registry;

    requireCondition(
        registry.registerValidator(record).accepted(),
        "Initial registration should be accepted."
    );

    const auto duplicate =
        registry.registerValidator(record);

    requireCondition(
        duplicate.duplicate(),
        "Duplicate registration with same key should be treated as duplicate."
    );

    requireCondition(
        registry.size() == 1U,
        "Duplicate registration should not increase registry size."
    );
}

void testConflictingPublicKeyIsRejected() {
    const PublicKey originalKey =
        publicKey("d-original");

    const PublicKey conflictingKey =
        publicKey("d-conflict");

    const ValidatorRegistrationRecord originalRecord =
        registrationFor(
            originalKey,
            4,
            "metadata-hash-d",
            kTimestamp + 30
        );

    const ValidatorRegistrationRecord conflictingRecord(
        originalRecord.validatorAddress(),
        conflictingKey,
        4,
        "metadata-hash-d-conflict",
        kTimestamp + 31
    );

    ValidatorRegistry registry;

    requireCondition(
        registry.registerValidator(originalRecord).accepted(),
        "Original registration should be accepted."
    );

    const auto conflict =
        registry.registerValidator(conflictingRecord);

    requireCondition(
        conflict.status() == ValidatorRegistryUpdateStatus::INVALID_RECORD ||
        conflict.status() == ValidatorRegistryUpdateStatus::CONFLICTING_PUBLIC_KEY,
        "Conflicting validator public key should be rejected."
    );

    requireCondition(
        registry.verifyValidatorIdentity(
            originalRecord.validatorAddress(),
            originalKey
        ),
        "Original identity should remain valid after conflicting attempt."
    );
}

void testInvalidAddressPublicKeyBindingIsRejected() {
    const PublicKey keyA =
        publicKey("e-a");

    const PublicKey keyB =
        publicKey("e-b");

    const Address addressFromA =
        AddressDerivation::deriveFromPublicKey(keyA);

    const ValidatorRegistrationRecord invalidRecord(
        addressFromA.value(),
        keyB,
        5,
        "metadata-hash-e",
        kTimestamp + 40
    );

    requireCondition(
        !invalidRecord.isValid(),
        "Record with address derived from another key should be invalid."
    );

    ValidatorRegistry registry;

    const auto result =
        registry.registerValidator(invalidRecord);

    requireCondition(
        !result.success(),
        "Registry should reject invalid address/key binding."
    );
}

void testDeactivateValidator() {
    const PublicKey key =
        publicKey("f");

    const ValidatorRegistrationRecord record =
        registrationFor(
            key,
            6,
            "metadata-hash-f",
            kTimestamp + 50
        );

    ValidatorRegistry registry;

    requireCondition(
        registry.registerValidator(record).accepted(),
        "Registration should be accepted before deactivation."
    );

    const auto result =
        registry.deactivateValidator(
            record.validatorAddress(),
            kTimestamp + 60
        );

    requireCondition(
        result.deactivated(),
        "Deactivation should succeed."
    );

    requireCondition(
        !registry.isActiveValidator(record.validatorAddress()),
        "Validator should not be active after deactivation."
    );

    requireCondition(
        !registry.verifyValidatorIdentity(
            record.validatorAddress(),
            key
        ),
        "Deactivated validator identity should not verify as active."
    );

    requireCondition(
        registry.activeValidatorAddresses().empty(),
        "No active validator addresses should remain."
    );
}

void testValidatorSetHistoryRejectsHeightGaps() {
    ValidatorRegistry registry;
    requireCondition(
        registry.registerValidator(registrationFor(
            publicKey("history"),
            1,
            "metadata-hash-history",
            kTimestamp
        )).accepted(),
        "History test validator registration should succeed."
    );

    ValidatorSetHistory history;
    requireCondition(
        history.recordSet(2, registry),
        "Validator-set history can start at any height (e.g., from snapshot)."
    );
    requireCondition(
        history.recordSet(3, registry),
        "Validator-set history must accept contiguous heights."
    );
    requireCondition(
        !history.recordSet(5, registry) && history.isValid(),
        "Validator-set history must reject gaps without corrupting prior history."
    );
}

void testStakeSplittingCannotIncreaseQuorumPower() {
    constexpr std::uint64_t oneNodo =
        ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS;
    const auto attackerA = registrationFor(publicKey("split-a"), 1,
                                           "metadata-split-a", kTimestamp);
    const auto attackerB = registrationFor(publicKey("split-b"), 1,
                                           "metadata-split-b", kTimestamp);
    const auto honest = registrationFor(publicKey("split-honest"), 1,
                                        "metadata-split-honest", kTimestamp);

    ValidatorRegistry concentrated;
    requireCondition(concentrated.registerValidator(attackerA, 30 * oneNodo,
                                                    attackerA.validatorAddress()).accepted(),
                     "concentrated attacker registration should succeed");
    requireCondition(concentrated.registerValidator(honest, 60 * oneNodo,
                                                    honest.validatorAddress()).accepted(),
                     "honest registration should succeed");

    ValidatorRegistry split;
    requireCondition(split.registerValidator(attackerA, 15 * oneNodo,
                                            attackerA.validatorAddress()).accepted(),
                     "first split registration should succeed");
    requireCondition(split.registerValidator(attackerB, 15 * oneNodo,
                                            attackerB.validatorAddress()).accepted(),
                     "second split registration should succeed");
    requireCondition(split.registerValidator(honest, 60 * oneNodo,
                                            honest.validatorAddress()).accepted(),
                     "honest registration in split set should succeed");

    const std::uint64_t concentratedAttacker =
        concentrated.consensusWeightFor(attackerA.validatorAddress());
    const std::uint64_t splitAttacker =
        split.consensusWeightFor(attackerA.validatorAddress()) +
        split.consensusWeightFor(attackerB.validatorAddress());
    requireCondition(concentratedAttacker == splitAttacker &&
                         concentratedAttacker == 30 * oneNodo,
                     "splitting keys must not create attacker voting power");
    requireCondition(concentrated.totalConsensusWeight() ==
                         split.totalConsensusWeight(),
                     "splitting keys must not change total quorum weight");
    requireCondition(split.eligibleValidatorAddresses().size() == 3,
                     "split keys must not displace the honest validator");
    requireCondition(
        nodo::consensus::QuorumCertificateBuilder::requiredVotingWeight(
            concentrated.totalConsensusWeight(), 2, 3) ==
            nodo::consensus::QuorumCertificateBuilder::requiredVotingWeight(
                split.totalConsensusWeight(), 2, 3),
        "splitting keys must not change the quorum threshold");
}

void testInvalidStakeAndAggregateOverflowFailClosed() {
    const auto belowMinimum = registrationFor(publicKey("below-minimum"), 1,
                                               "metadata-below", kTimestamp);
    ValidatorRegistry registry;
    requireCondition(!registry.registerValidator(
                         belowMinimum,
                         ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS - 1,
                         belowMinimum.validatorAddress()).success(),
                     "a validator below the minimum must be rejected");

    const auto first = registrationFor(publicKey("overflow-a"), 1,
                                        "metadata-overflow-a", kTimestamp);
    const auto second = registrationFor(publicKey("overflow-b"), 1,
                                         "metadata-overflow-b", kTimestamp);
    const auto third = registrationFor(publicKey("overflow-c"), 1,
                                        "metadata-overflow-c", kTimestamp);
    constexpr std::uint64_t largestAmount =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    requireCondition(registry.registerValidator(first, largestAmount,
                                                first.validatorAddress()).accepted(),
                     "largest representable stake should be accepted");
    requireCondition(registry.registerValidator(second, largestAmount,
                                                second.validatorAddress()).accepted(),
                     "two largest stakes still fit the aggregate");
    const auto rejected = registry.registerValidator(
        third, ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS,
        third.validatorAddress());
    requireCondition(!rejected.success() && registry.size() == 2 &&
                         registry.isValid(),
                     "aggregate overflow must reject and preserve registry");

    ValidatorRegistry withPending = registry;
    requireCondition(withPending.registerPendingValidator(
                         third, ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS,
                         third.validatorAddress()).accepted(),
                     "pending stake does not yet enter voting weight");
    const auto rejectedActivation = withPending.activateValidator(
        third.validatorAddress(), 1, kTimestamp + 1);
    requireCondition(!rejectedActivation.success() && withPending.isValid() &&
                         !withPending.isEligibleForConsensus(third.validatorAddress()),
                     "overflowing activation must leave the validator pending");

    ValidatorRegistry withJailed = registry;
    requireCondition(withJailed.jailValidator(first.validatorAddress(), 1,
                                             kTimestamp + 1).success(),
                     "validator should be jailed before overflow test");
    requireCondition(withJailed.registerValidator(
                         third, ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS,
                         third.validatorAddress()).accepted(),
                     "released voting headroom should allow another validator");
    const auto rejectedUnjail = withJailed.unjailValidator(
        first.validatorAddress(), 1, kTimestamp + 2);
    requireCondition(!rejectedUnjail.success() && withJailed.isValid() &&
                         !withJailed.isEligibleForConsensus(first.validatorAddress()),
                     "overflowing unjail must leave the validator jailed");

    ValidatorRegistry withHeadroom;
    requireCondition(withHeadroom.registerValidator(
                         first, largestAmount, first.validatorAddress()).accepted(),
                     "first bounded stake should register");
    requireCondition(withHeadroom.registerValidator(
                         second,
                         largestAmount -
                             ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS,
                         second.validatorAddress()).accepted(),
                     "second bounded stake should register");
    requireCondition(withHeadroom.registerValidator(
                         third, ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS,
                         third.validatorAddress()).accepted(),
                     "exactly bounded total should register");
    const auto rejectedUpdate = withHeadroom.updateStake(
        third.validatorAddress(),
        ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS + 2,
        kTimestamp + 1);
    requireCondition(!rejectedUpdate.success() && withHeadroom.isValid() &&
                         withHeadroom.consensusWeightFor(third.validatorAddress()) ==
                             ValidatorRegistry::MIN_VALIDATOR_STAKE_RAW_UNITS,
                     "overflowing stake update must preserve previous weight");
}

} // namespace

int main() {
    try {
        testRegistrationBindsAddressToPublicKey();
        testRegistryAcceptsAndVerifiesValidatorIdentity();
        testDuplicateRegistrationIsSafeNoOp();
        testConflictingPublicKeyIsRejected();
        testInvalidAddressPublicKeyBindingIsRejected();
        testDeactivateValidator();
        testValidatorSetHistoryRejectsHeightGaps();
        testStakeSplittingCannotIncreaseQuorumPower();
        testInvalidStakeAndAggregateOverflowFailClosed();

        std::cout << "Nodo validator registry tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo validator registry tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
