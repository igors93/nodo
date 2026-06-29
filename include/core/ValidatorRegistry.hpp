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
    PENDING_ACTIVATION,
    ACTIVE,
    JAILED,
    EXIT_REQUESTED,
    EXITED,
    DEACTIVATED
};

ValidatorRegistrationStatus validatorRegistrationStatusFromString(
    const std::string& value
);

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

    ValidatorRegistryEntry(
        ValidatorRegistrationRecord registrationRecord,
        ValidatorRegistrationStatus status,
        std::int64_t lastUpdatedAt,
        std::uint64_t stakeAmount,
        std::uint64_t jailUntilEpoch,
        std::uint64_t exitRequestHeight,
        std::string ownerAddress
    );

    const ValidatorRegistrationRecord& registrationRecord() const;
    ValidatorRegistrationStatus status() const;
    std::int64_t lastUpdatedAt() const;
    std::uint64_t stakeAmount() const;
    std::uint64_t jailUntilEpoch() const;
    std::uint64_t exitRequestHeight() const;
    const std::string& ownerAddress() const;

    bool active() const;
    bool eligibleForConsensus() const;
    bool jailed() const;
    bool exited() const;

    bool isValid() const;

    std::string serialize() const;

private:
    ValidatorRegistrationRecord m_registrationRecord;
    ValidatorRegistrationStatus m_status;
    std::int64_t m_lastUpdatedAt;
    std::uint64_t m_stakeAmount;
    std::uint64_t m_jailUntilEpoch;
    std::uint64_t m_exitRequestHeight;
    std::string m_ownerAddress;
};

enum class ValidatorRegistryUpdateStatus {
    ACCEPTED,
    DUPLICATE,
    DEACTIVATED,
    JAILED,
    UNJAILED,
    EXIT_REQUESTED,
    ACTIVATED,
    CONFLICTING_PUBLIC_KEY,
    INVALID_RECORD,
    INVALID_REGISTRY,
    ALREADY_ACTIVE,
    ALREADY_JAILED,
    INVALID_STATUS_TRANSITION,
    INSUFFICIENT_STAKE
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

    static ValidatorRegistryUpdateResult jailed(
        ValidatorRegistryEntry entry
    );

    static ValidatorRegistryUpdateResult unjailed(
        ValidatorRegistryEntry entry
    );

    static ValidatorRegistryUpdateResult exitRequested(
        ValidatorRegistryEntry entry
    );

    static ValidatorRegistryUpdateResult activated(
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
    static constexpr std::uint64_t MIN_VALIDATOR_STAKE_RAW_UNITS = 1'000'000;

    ValidatorRegistry();

    ValidatorRegistryUpdateResult registerValidator(
        const ValidatorRegistrationRecord& registrationRecord
    );

    ValidatorRegistryUpdateResult registerPendingValidator(
        const ValidatorRegistrationRecord& registrationRecord,
        std::uint64_t stakeAmount,
        const std::string& ownerAddress
    );

    ValidatorRegistryUpdateResult deactivateValidator(
        const std::string& validatorAddress,
        std::int64_t timestamp
    );

    ValidatorRegistryUpdateResult activateValidator(
        const std::string& validatorAddress,
        std::uint64_t currentEpoch,
        std::int64_t timestamp
    );

    ValidatorRegistryUpdateResult jailValidator(
        const std::string& validatorAddress,
        std::uint64_t jailUntilEpoch,
        std::int64_t timestamp
    );

    ValidatorRegistryUpdateResult unjailValidator(
        const std::string& validatorAddress,
        std::uint64_t currentEpoch,
        std::int64_t timestamp
    );

    ValidatorRegistryUpdateResult requestExit(
        const std::string& validatorAddress,
        std::uint64_t requestHeight,
        std::int64_t timestamp
    );

    ValidatorRegistryUpdateResult completeExit(
        const std::string& validatorAddress,
        std::int64_t timestamp
    );

    ValidatorRegistryUpdateResult updateStake(
        const std::string& validatorAddress,
        std::uint64_t newStakeAmount,
        std::int64_t timestamp
    );

    bool hasValidator(
        const std::string& validatorAddress
    ) const;

    bool isActiveValidator(
        const std::string& validatorAddress
    ) const;

    bool isEligibleForConsensus(
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
    std::vector<std::string> eligibleValidatorAddresses() const;
    std::vector<std::string> jailedValidatorAddresses() const;
    std::vector<std::string> pendingValidatorAddresses() const;
    std::vector<std::string> exitRequestedValidatorAddresses() const;

    std::size_t size() const;
    std::size_t activeCount() const;

    bool isValid() const;

    std::string serialize() const;

private:
    std::map<std::string, ValidatorRegistryEntry> m_entries;
};

/*
 * Immutable-by-height validator-set snapshots used to verify historical QCs.
 * A QC must be checked against the set that was active for its own height,
 * never against the node's current registry.
 */
class ValidatorSetHistory {
public:
    bool recordSet(std::uint64_t height, const ValidatorRegistry& registry);
    bool hasSet(std::uint64_t height) const;
    const ValidatorRegistry& setAt(std::uint64_t height) const;
    std::uint64_t highestRecordedHeight() const;
    bool isValid() const;
    std::string serialize() const;

private:
    std::map<std::uint64_t, ValidatorRegistry> m_setsByHeight;
};

} // namespace nodo::core

#endif
