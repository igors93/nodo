#include "core/ValidatorRegistry.hpp"
#include "crypto/Address.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PublicKey.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::core::ValidatorRegistrationRecord;
using nodo::core::ValidatorRegistry;
using nodo::core::ValidatorRegistryUpdateStatus;
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

} // namespace

int main() {
    try {
        testRegistrationBindsAddressToPublicKey();
        testRegistryAcceptsAndVerifiesValidatorIdentity();
        testDuplicateRegistrationIsSafeNoOp();
        testConflictingPublicKeyIsRejected();
        testInvalidAddressPublicKeyBindingIsRejected();
        testDeactivateValidator();

        std::cout << "Nodo validator registry tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Nodo validator registry tests failed: "
                  << error.what()
                  << "\n";
        return 1;
    }
}
