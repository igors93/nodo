#ifndef NODO_CORE_VALIDATOR_REGISTRY_HPP
#define NODO_CORE_VALIDATOR_REGISTRY_HPP

#include "crypto/Address.hpp"
#include "crypto/PublicKey.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nodo::core {

enum class ValidatorRegistrationStatus {
    UNKNOWN,
    ACTIVE,
    DEACTIVATED
};

std::string validatorRegistrationStatusToString(
    ValidatorRegistrationStatus status
);

/*
 * ValidatorRegistrationRecord binds a Nodo address to a validator public key.
 *
 * Security principle:
 * A validator address must be derived from the public key it claims to use.
 * This prevents a validator from registering someone else's address with its
 * own key.
 */
class ValidatorRegistrationRecord {
public:
    ValidatorRegistrationRecord();

    ValidatorRegistrationRecord(
        std::string validatorAddress,
        crypto::PublicKey validatorPublicKey,
        std::uint64_t activationEpoch,
        std::string metadataHash,
        std::int64_t registeredAt
    );

    const std::string& validatorAddress() const;
    const crypto::PublicKey& validatorPublicKey() const;
    std::uint64_t activationEpoch() const;
    const std::string& metadataHash() const;
    std::int64_t registeredAt() const;

    std::string publicKeyFingerprint() const;

    bool isValid() const;

    std::string deterministicId() const;

    std::string serialize() const;

private:
    std::string m_validatorAddress;
    crypto::PublicKey m_validatorPublicKey;
    std::uint64_t m_activationEpoch;
    std::string m_metadataHash;
    std::int64_t m_registeredAt;
};

class ValidatorRegistryEntry {
public:
    ValidatorRegistryEntry();

    ValidatorRegistryEntry(
        ValidatorRegistrationRecord registrationRecord,
        ValidatorRegistrationStatus status,
        std::int64_t lastUpdatedAt
    );

    const ValidatorRegistrationRecord& registrationRecord() const;
    ValidatorRegistrationStatus status() const;
    std::int64_t lastUpdatedAt() const;

    bool active() const;

    bool isValid() const;

    std::string serialize() const;

private:
    ValidatorRegistrationRecord m_registrationRecord;
    ValidatorRegistrationStatus m_status;
    std::int64_t m_lastUpdatedAt;
};

enum class ValidatorRegistryUpdateStatus {
    ACCEPTED,
    DUPLICATE,
    DEACTIVATED,
    CONFLICTING_PUBLIC_KEY,
    INVALID_RECORD,
    INVALID_REGISTRY
};

std::string validatorRegistryUpdateStatusToString(
    ValidatorRegistryUpdateStatus status
);

class ValidatorRegistryUpdateResult {
public:
    ValidatorRegistryUpdateResult();

    static ValidatorRegistryUpdateResult accepted(
        ValidatorRegistryEntry entry
    );

    static ValidatorRegistryUpdateResult duplicate(
        ValidatorRegistryEntry entry
    );

    static ValidatorRegistryUpdateResult deactivated(
        ValidatorRegistryEntry entry
    );

    static ValidatorRegistryUpdateResult rejected(
        ValidatorRegistryUpdateStatus status,
        std::string reason
    );

    ValidatorRegistryUpdateStatus status() const;
    const std::string& reason() const;
    const ValidatorRegistryEntry& entry() const;

    bool accepted() const;
    bool duplicate() const;
    bool deactivated() const;
    bool success() const;

    std::string serialize() const;

private:
    ValidatorRegistryUpdateStatus m_status;
    std::string m_reason;
    ValidatorRegistryEntry m_entry;
};

/*
 * ValidatorRegistry is the local validator identity registry.
 *
 * Important:
 * This registry does not replace consensus. It is the identity foundation that
 * future proposal, vote and slashing rules should use before accepting a
 * validator action.
 */
class ValidatorRegistry {
public:
    ValidatorRegistry();

    ValidatorRegistryUpdateResult registerValidator(
        const ValidatorRegistrationRecord& registrationRecord
    );

    ValidatorRegistryUpdateResult deactivateValidator(
        const std::string& validatorAddress,
        std::int64_t timestamp
    );

    bool hasValidator(
        const std::string& validatorAddress
    ) const;

    bool isActiveValidator(
        const std::string& validatorAddress
    ) const;

    bool verifyValidatorIdentity(
        const std::string& validatorAddress,
        const crypto::PublicKey& validatorPublicKey
    ) const;

    const ValidatorRegistryEntry* entryForAddress(
        const std::string& validatorAddress
    ) const;

    std::vector<std::string> activeValidatorAddresses() const;

    std::size_t size() const;
    std::size_t activeCount() const;

    bool isValid() const;

    std::string serialize() const;

private:
    std::map<std::string, ValidatorRegistryEntry> m_entries;
};

} // namespace nodo::core

#endif
