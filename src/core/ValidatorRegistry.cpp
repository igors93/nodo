#include "core/ValidatorRegistry.hpp"

#include "crypto/AddressDerivation.hpp"
#include "crypto/hash.h"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::core {

namespace {

bool isSafeScalar(
    const std::string& value
) {
    if (value.empty()) {
        return false;
    }

    for (const char character : value) {
        if (character == ';' ||
            character == '{' ||
            character == '}' ||
            character == '[' ||
            character == ']' ||
            character == '\n' ||
            character == '\r' ||
            character == '\t') {
            return false;
        }
    }

    return true;
}

std::string hashString(
    const std::string& value
) {
    char output[NODO_HASH_BUFFER_SIZE] = {0};

    nodo_hash_string(
        value.c_str(),
        output,
        sizeof(output)
    );

    return std::string(output);
}

} // namespace

std::string validatorRegistrationStatusToString(
    ValidatorRegistrationStatus status
) {
    switch (status) {
        case ValidatorRegistrationStatus::PENDING_ACTIVATION:
            return "PENDING_ACTIVATION";
        case ValidatorRegistrationStatus::ACTIVE:
            return "ACTIVE";
        case ValidatorRegistrationStatus::JAILED:
            return "JAILED";
        case ValidatorRegistrationStatus::EXIT_REQUESTED:
            return "EXIT_REQUESTED";
        case ValidatorRegistrationStatus::EXITED:
            return "EXITED";
        case ValidatorRegistrationStatus::DEACTIVATED:
            return "DEACTIVATED";
        case ValidatorRegistrationStatus::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

ValidatorRegistrationStatus validatorRegistrationStatusFromString(
    const std::string& value
) {
    if (value == "PENDING_ACTIVATION") return ValidatorRegistrationStatus::PENDING_ACTIVATION;
    if (value == "ACTIVE")             return ValidatorRegistrationStatus::ACTIVE;
    if (value == "JAILED")             return ValidatorRegistrationStatus::JAILED;
    if (value == "EXIT_REQUESTED")     return ValidatorRegistrationStatus::EXIT_REQUESTED;
    if (value == "EXITED")             return ValidatorRegistrationStatus::EXITED;
    if (value == "DEACTIVATED")        return ValidatorRegistrationStatus::DEACTIVATED;
    return ValidatorRegistrationStatus::UNKNOWN;
}

ValidatorRegistrationRecord::ValidatorRegistrationRecord()
    : m_validatorAddress(""),
      m_validatorPublicKey(),
      m_activationEpoch(0),
      m_metadataHash(""),
      m_registeredAt(0) {}

ValidatorRegistrationRecord::ValidatorRegistrationRecord(
    std::string validatorAddress,
    crypto::PublicKey validatorPublicKey,
    std::uint64_t activationEpoch,
    std::string metadataHash,
    std::int64_t registeredAt
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_validatorPublicKey(std::move(validatorPublicKey)),
      m_activationEpoch(activationEpoch),
      m_metadataHash(std::move(metadataHash)),
      m_registeredAt(registeredAt) {}

const std::string& ValidatorRegistrationRecord::validatorAddress() const {
    return m_validatorAddress;
}

const crypto::PublicKey& ValidatorRegistrationRecord::validatorPublicKey() const {
    return m_validatorPublicKey;
}

std::uint64_t ValidatorRegistrationRecord::activationEpoch() const {
    return m_activationEpoch;
}

const std::string& ValidatorRegistrationRecord::metadataHash() const {
    return m_metadataHash;
}

std::int64_t ValidatorRegistrationRecord::registeredAt() const {
    return m_registeredAt;
}

std::string ValidatorRegistrationRecord::publicKeyFingerprint() const {
    return m_validatorPublicKey.fingerprint();
}

bool ValidatorRegistrationRecord::isValid() const {
    if (!isSafeScalar(m_validatorAddress) ||
        !isSafeScalar(m_metadataHash)) {
        return false;
    }

    if (!m_validatorPublicKey.isValid()) {
        return false;
    }

    if (!isSafeScalar(m_validatorPublicKey.keyMaterial())) {
        return false;
    }

    if (m_activationEpoch == 0 ||
        m_registeredAt <= 0) {
        return false;
    }

    const crypto::Address address =
        crypto::Address::fromString(m_validatorAddress);

    if (!address.isValid()) {
        return false;
    }

    /*
     * Binding rule:
     * A validator address must be derived from the validator public key.
     */
    if (!crypto::AddressDerivation::verifyAddressForPublicKey(
            address,
            m_validatorPublicKey
        )) {
        return false;
    }

    return true;
}

std::string ValidatorRegistrationRecord::deterministicId() const {
    if (!isValid()) {
        return "";
    }

    return hashString(serialize());
}

std::string ValidatorRegistrationRecord::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorRegistrationRecord{"
        << "validatorAddress=" << m_validatorAddress
        << ";publicKey=" << m_validatorPublicKey.serialize()
        << ";publicKeyFingerprint=" << publicKeyFingerprint()
        << ";activationEpoch=" << m_activationEpoch
        << ";metadataHash=" << m_metadataHash
        << ";registeredAt=" << m_registeredAt
        << "}";

    return oss.str();
}

ValidatorRegistryEntry::ValidatorRegistryEntry()
    : m_registrationRecord(),
      m_status(ValidatorRegistrationStatus::UNKNOWN),
      m_lastUpdatedAt(0),
      m_stakeAmount(0),
      m_jailUntilEpoch(0),
      m_exitRequestHeight(0),
      m_ownerAddress("") {}

ValidatorRegistryEntry::ValidatorRegistryEntry(
    ValidatorRegistrationRecord registrationRecord,
    ValidatorRegistrationStatus status,
    std::int64_t lastUpdatedAt
)
    : m_registrationRecord(std::move(registrationRecord)),
      m_status(status),
      m_lastUpdatedAt(lastUpdatedAt),
      m_stakeAmount(0),
      m_jailUntilEpoch(0),
      m_exitRequestHeight(0),
      m_ownerAddress("") {}

ValidatorRegistryEntry::ValidatorRegistryEntry(
    ValidatorRegistrationRecord registrationRecord,
    ValidatorRegistrationStatus status,
    std::int64_t lastUpdatedAt,
    std::uint64_t stakeAmount,
    std::uint64_t jailUntilEpoch,
    std::uint64_t exitRequestHeight,
    std::string ownerAddress
)
    : m_registrationRecord(std::move(registrationRecord)),
      m_status(status),
      m_lastUpdatedAt(lastUpdatedAt),
      m_stakeAmount(stakeAmount),
      m_jailUntilEpoch(jailUntilEpoch),
      m_exitRequestHeight(exitRequestHeight),
      m_ownerAddress(std::move(ownerAddress)) {}

const ValidatorRegistrationRecord& ValidatorRegistryEntry::registrationRecord() const {
    return m_registrationRecord;
}

ValidatorRegistrationStatus ValidatorRegistryEntry::status() const {
    return m_status;
}

std::int64_t ValidatorRegistryEntry::lastUpdatedAt() const {
    return m_lastUpdatedAt;
}

std::uint64_t ValidatorRegistryEntry::stakeAmount() const {
    return m_stakeAmount;
}

std::uint64_t ValidatorRegistryEntry::jailUntilEpoch() const {
    return m_jailUntilEpoch;
}

std::uint64_t ValidatorRegistryEntry::exitRequestHeight() const {
    return m_exitRequestHeight;
}

const std::string& ValidatorRegistryEntry::ownerAddress() const {
    return m_ownerAddress;
}

bool ValidatorRegistryEntry::eligibleForConsensus() const {
    return m_status == ValidatorRegistrationStatus::ACTIVE;
}

bool ValidatorRegistryEntry::jailed() const {
    return m_status == ValidatorRegistrationStatus::JAILED;
}

bool ValidatorRegistryEntry::exited() const {
    return m_status == ValidatorRegistrationStatus::EXITED ||
           m_status == ValidatorRegistrationStatus::DEACTIVATED;
}

bool ValidatorRegistryEntry::active() const {
    return m_status == ValidatorRegistrationStatus::ACTIVE;
}

bool ValidatorRegistryEntry::isValid() const {
    if (!m_registrationRecord.isValid()) {
        return false;
    }

    if (m_status == ValidatorRegistrationStatus::UNKNOWN) {
        return false;
    }

    if (m_lastUpdatedAt <= 0) {
        return false;
    }

    if (m_lastUpdatedAt < m_registrationRecord.registeredAt()) {
        return false;
    }

    return true;
}

std::string ValidatorRegistryEntry::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorRegistryEntry{"
        << "status=" << validatorRegistrationStatusToString(m_status)
        << ";lastUpdatedAt=" << m_lastUpdatedAt
        << ";stakeAmount=" << m_stakeAmount
        << ";jailUntilEpoch=" << m_jailUntilEpoch
        << ";exitRequestHeight=" << m_exitRequestHeight
        << ";ownerAddress=" << m_ownerAddress
        << ";registration=" << m_registrationRecord.serialize()
        << "}";

    return oss.str();
}

std::string validatorRegistryUpdateStatusToString(
    ValidatorRegistryUpdateStatus status
) {
    switch (status) {
        case ValidatorRegistryUpdateStatus::ACCEPTED:            return "ACCEPTED";
        case ValidatorRegistryUpdateStatus::DUPLICATE:           return "DUPLICATE";
        case ValidatorRegistryUpdateStatus::DEACTIVATED:         return "DEACTIVATED";
        case ValidatorRegistryUpdateStatus::JAILED:              return "JAILED";
        case ValidatorRegistryUpdateStatus::UNJAILED:            return "UNJAILED";
        case ValidatorRegistryUpdateStatus::EXIT_REQUESTED:      return "EXIT_REQUESTED";
        case ValidatorRegistryUpdateStatus::ACTIVATED:           return "ACTIVATED";
        case ValidatorRegistryUpdateStatus::CONFLICTING_PUBLIC_KEY: return "CONFLICTING_PUBLIC_KEY";
        case ValidatorRegistryUpdateStatus::INVALID_RECORD:      return "INVALID_RECORD";
        case ValidatorRegistryUpdateStatus::INVALID_REGISTRY:    return "INVALID_REGISTRY";
        case ValidatorRegistryUpdateStatus::ALREADY_ACTIVE:      return "ALREADY_ACTIVE";
        case ValidatorRegistryUpdateStatus::ALREADY_JAILED:      return "ALREADY_JAILED";
        case ValidatorRegistryUpdateStatus::INVALID_STATUS_TRANSITION: return "INVALID_STATUS_TRANSITION";
        case ValidatorRegistryUpdateStatus::INSUFFICIENT_STAKE:  return "INSUFFICIENT_STAKE";
        default:                                                  return "INVALID_REGISTRY";
    }
}

ValidatorRegistryUpdateResult::ValidatorRegistryUpdateResult()
    : m_status(ValidatorRegistryUpdateStatus::INVALID_REGISTRY),
      m_reason("Uninitialized registry update result."),
      m_entry() {}

ValidatorRegistryUpdateResult ValidatorRegistryUpdateResult::accepted(
    ValidatorRegistryEntry entry
) {
    ValidatorRegistryUpdateResult result;
    result.m_status = ValidatorRegistryUpdateStatus::ACCEPTED;
    result.m_reason = "";
    result.m_entry = std::move(entry);
    return result;
}

ValidatorRegistryUpdateResult ValidatorRegistryUpdateResult::duplicate(
    ValidatorRegistryEntry entry
) {
    ValidatorRegistryUpdateResult result;
    result.m_status = ValidatorRegistryUpdateStatus::DUPLICATE;
    result.m_reason = "Validator registration already exists.";
    result.m_entry = std::move(entry);
    return result;
}

ValidatorRegistryUpdateResult ValidatorRegistryUpdateResult::deactivated(
    ValidatorRegistryEntry entry
) {
    ValidatorRegistryUpdateResult result;
    result.m_status = ValidatorRegistryUpdateStatus::DEACTIVATED;
    result.m_reason = "";
    result.m_entry = std::move(entry);
    return result;
}

ValidatorRegistryUpdateResult ValidatorRegistryUpdateResult::jailed(
    ValidatorRegistryEntry entry
) {
    ValidatorRegistryUpdateResult result;
    result.m_status = ValidatorRegistryUpdateStatus::JAILED;
    result.m_reason = "";
    result.m_entry = std::move(entry);
    return result;
}

ValidatorRegistryUpdateResult ValidatorRegistryUpdateResult::unjailed(
    ValidatorRegistryEntry entry
) {
    ValidatorRegistryUpdateResult result;
    result.m_status = ValidatorRegistryUpdateStatus::UNJAILED;
    result.m_reason = "";
    result.m_entry = std::move(entry);
    return result;
}

ValidatorRegistryUpdateResult ValidatorRegistryUpdateResult::exitRequested(
    ValidatorRegistryEntry entry
) {
    ValidatorRegistryUpdateResult result;
    result.m_status = ValidatorRegistryUpdateStatus::EXIT_REQUESTED;
    result.m_reason = "";
    result.m_entry = std::move(entry);
    return result;
}

ValidatorRegistryUpdateResult ValidatorRegistryUpdateResult::activated(
    ValidatorRegistryEntry entry
) {
    ValidatorRegistryUpdateResult result;
    result.m_status = ValidatorRegistryUpdateStatus::ACTIVATED;
    result.m_reason = "";
    result.m_entry = std::move(entry);
    return result;
}

ValidatorRegistryUpdateResult ValidatorRegistryUpdateResult::rejected(
    ValidatorRegistryUpdateStatus status,
    std::string reason
) {
    ValidatorRegistryUpdateResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

ValidatorRegistryUpdateStatus ValidatorRegistryUpdateResult::status() const {
    return m_status;
}

const std::string& ValidatorRegistryUpdateResult::reason() const {
    return m_reason;
}

const ValidatorRegistryEntry& ValidatorRegistryUpdateResult::entry() const {
    return m_entry;
}

bool ValidatorRegistryUpdateResult::accepted() const {
    return m_status == ValidatorRegistryUpdateStatus::ACCEPTED;
}

bool ValidatorRegistryUpdateResult::duplicate() const {
    return m_status == ValidatorRegistryUpdateStatus::DUPLICATE;
}

bool ValidatorRegistryUpdateResult::deactivated() const {
    return m_status == ValidatorRegistryUpdateStatus::DEACTIVATED;
}

bool ValidatorRegistryUpdateResult::success() const {
    return m_status == ValidatorRegistryUpdateStatus::ACCEPTED ||
           m_status == ValidatorRegistryUpdateStatus::DUPLICATE ||
           m_status == ValidatorRegistryUpdateStatus::DEACTIVATED ||
           m_status == ValidatorRegistryUpdateStatus::JAILED ||
           m_status == ValidatorRegistryUpdateStatus::UNJAILED ||
           m_status == ValidatorRegistryUpdateStatus::EXIT_REQUESTED ||
           m_status == ValidatorRegistryUpdateStatus::ACTIVATED;
}

std::string ValidatorRegistryUpdateResult::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorRegistryUpdateResult{"
        << "status=" << validatorRegistryUpdateStatusToString(m_status)
        << ";reason=" << m_reason
        << ";entry=" << (m_entry.isValid() ? m_entry.serialize() : "INVALID")
        << "}";

    return oss.str();
}

ValidatorRegistry::ValidatorRegistry()
    : m_entries() {}

ValidatorRegistryUpdateResult ValidatorRegistry::registerValidator(
    const ValidatorRegistrationRecord& registrationRecord
) {
    if (!registrationRecord.isValid()) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Invalid validator registration record."
        );
    }

    const std::string& address =
        registrationRecord.validatorAddress();

    const auto existing =
        m_entries.find(address);

    if (existing != m_entries.end()) {
        if (!existing->second.isValid()) {
            return ValidatorRegistryUpdateResult::rejected(
                ValidatorRegistryUpdateStatus::INVALID_REGISTRY,
                "Existing validator registry entry is invalid."
            );
        }

        if (existing->second.registrationRecord().publicKeyFingerprint() ==
            registrationRecord.publicKeyFingerprint()) {
            return ValidatorRegistryUpdateResult::duplicate(
                existing->second
            );
        }

        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::CONFLICTING_PUBLIC_KEY,
            "Validator address is already bound to a different public key."
        );
    }

    ValidatorRegistryEntry entry(
        registrationRecord,
        ValidatorRegistrationStatus::ACTIVE,
        registrationRecord.registeredAt()
    );

    if (!entry.isValid()) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Validator registry entry would be invalid."
        );
    }

    m_entries.emplace(address, entry);

    if (!isValid()) {
        m_entries.erase(address);

        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_REGISTRY,
            "Validator registry failed post-registration audit."
        );
    }

    return ValidatorRegistryUpdateResult::accepted(entry);
}

ValidatorRegistryUpdateResult ValidatorRegistry::registerPendingValidator(
    const ValidatorRegistrationRecord& registrationRecord,
    std::uint64_t stakeAmount,
    const std::string& ownerAddress
) {
    if (!registrationRecord.isValid() ||
        stakeAmount < MIN_VALIDATOR_STAKE_RAW_UNITS ||
        !isSafeScalar(ownerAddress)) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Pending validator registration is invalid or below minimum stake."
        );
    }
    const std::string& address = registrationRecord.validatorAddress();
    if (m_entries.find(address) != m_entries.end()) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::DUPLICATE,
            "Validator is already registered."
        );
    }
    ValidatorRegistryEntry entry(
        registrationRecord,
        ValidatorRegistrationStatus::PENDING_ACTIVATION,
        registrationRecord.registeredAt(),
        stakeAmount,
        0,
        0,
        ownerAddress
    );
    if (!entry.isValid()) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Pending validator entry is invalid."
        );
    }
    m_entries.emplace(address, entry);
    if (!isValid()) {
        m_entries.erase(address);
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_REGISTRY,
            "Validator registry failed pending-registration audit."
        );
    }
    return ValidatorRegistryUpdateResult::accepted(entry);
}

ValidatorRegistryUpdateResult ValidatorRegistry::deactivateValidator(
    const std::string& validatorAddress,
    std::int64_t timestamp
) {
    if (!isSafeScalar(validatorAddress)) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Validator address is invalid."
        );
    }

    if (timestamp <= 0) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Deactivation timestamp must be positive."
        );
    }

    auto existing =
        m_entries.find(validatorAddress);

    if (existing == m_entries.end()) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Validator is not registered."
        );
    }

    ValidatorRegistryEntry updatedEntry(
        existing->second.registrationRecord(),
        ValidatorRegistrationStatus::DEACTIVATED,
        timestamp
    );

    if (!updatedEntry.isValid()) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Deactivated validator entry would be invalid."
        );
    }

    existing->second = updatedEntry;

    if (!isValid()) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_REGISTRY,
            "Validator registry failed post-deactivation audit."
        );
    }

    return ValidatorRegistryUpdateResult::deactivated(updatedEntry);
}

bool ValidatorRegistry::hasValidator(
    const std::string& validatorAddress
) const {
    return m_entries.find(validatorAddress) != m_entries.end();
}

bool ValidatorRegistry::isActiveValidator(
    const std::string& validatorAddress
) const {
    const auto entry =
        m_entries.find(validatorAddress);

    if (entry == m_entries.end()) {
        return false;
    }

    return entry->second.active();
}

bool ValidatorRegistry::verifyValidatorIdentity(
    const std::string& validatorAddress,
    const crypto::PublicKey& validatorPublicKey
) const {
    const auto entry =
        m_entries.find(validatorAddress);

    if (entry == m_entries.end()) {
        return false;
    }

    if (!entry->second.active()) {
        return false;
    }

    if (!validatorPublicKey.isValid()) {
        return false;
    }

    if (entry->second.registrationRecord().publicKeyFingerprint() !=
        validatorPublicKey.fingerprint()) {
        return false;
    }

    const crypto::Address address =
        crypto::Address::fromString(validatorAddress);

    return crypto::AddressDerivation::verifyAddressForPublicKey(
        address,
        validatorPublicKey
    );
}

const ValidatorRegistryEntry* ValidatorRegistry::entryForAddress(
    const std::string& validatorAddress
) const {
    const auto entry =
        m_entries.find(validatorAddress);

    if (entry == m_entries.end()) {
        return nullptr;
    }

    return &entry->second;
}

std::vector<std::string> ValidatorRegistry::activeValidatorAddresses() const {
    std::vector<std::string> addresses;

    for (const auto& [address, entry] : m_entries) {
        if (entry.active()) {
            addresses.push_back(address);
        }
    }

    return addresses;
}

std::vector<std::string> ValidatorRegistry::eligibleValidatorAddresses() const {
    std::vector<std::string> addresses;

    for (const auto& [address, entry] : m_entries) {
        if (entry.eligibleForConsensus()) {
            addresses.push_back(address);
        }
    }

    return addresses;
}

std::vector<std::string> ValidatorRegistry::jailedValidatorAddresses() const {
    std::vector<std::string> addresses;

    for (const auto& [address, entry] : m_entries) {
        if (entry.jailed()) {
            addresses.push_back(address);
        }
    }

    return addresses;
}

std::vector<std::string> ValidatorRegistry::pendingValidatorAddresses() const {
    std::vector<std::string> addresses;

    for (const auto& [address, entry] : m_entries) {
        if (entry.status() == ValidatorRegistrationStatus::PENDING_ACTIVATION) {
            addresses.push_back(address);
        }
    }

    return addresses;
}

std::vector<std::string> ValidatorRegistry::exitRequestedValidatorAddresses() const {
    std::vector<std::string> addresses;

    for (const auto& [address, entry] : m_entries) {
        if (entry.status() == ValidatorRegistrationStatus::EXIT_REQUESTED) {
            addresses.push_back(address);
        }
    }

    return addresses;
}

bool ValidatorRegistry::isEligibleForConsensus(
    const std::string& validatorAddress
) const {
    const auto it = m_entries.find(validatorAddress);
    if (it == m_entries.end()) {
        return false;
    }
    return it->second.eligibleForConsensus();
}

ValidatorRegistryUpdateResult ValidatorRegistry::activateValidator(
    const std::string& validatorAddress,
    std::uint64_t currentEpoch,
    std::int64_t timestamp
) {
    auto it = m_entries.find(validatorAddress);
    if (it == m_entries.end()) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Validator is not registered."
        );
    }

    const ValidatorRegistryEntry& existing = it->second;

    if (existing.status() == ValidatorRegistrationStatus::ACTIVE) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::ALREADY_ACTIVE,
            "Validator is already active."
        );
    }

    if (existing.status() != ValidatorRegistrationStatus::PENDING_ACTIVATION) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_STATUS_TRANSITION,
            "Only PENDING_ACTIVATION validators can be activated."
        );
    }

    if (existing.registrationRecord().activationEpoch() > currentEpoch) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_STATUS_TRANSITION,
            "Activation epoch has not been reached."
        );
    }

    ValidatorRegistryEntry updated(
        existing.registrationRecord(),
        ValidatorRegistrationStatus::ACTIVE,
        timestamp,
        existing.stakeAmount(),
        existing.jailUntilEpoch(),
        existing.exitRequestHeight(),
        existing.ownerAddress()
    );

    it->second = updated;
    return ValidatorRegistryUpdateResult::activated(updated);
}

ValidatorRegistryUpdateResult ValidatorRegistry::jailValidator(
    const std::string& validatorAddress,
    std::uint64_t jailUntilEpoch,
    std::int64_t timestamp
) {
    auto it = m_entries.find(validatorAddress);
    if (it == m_entries.end()) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Validator is not registered."
        );
    }

    const ValidatorRegistryEntry& existing = it->second;

    if (existing.status() == ValidatorRegistrationStatus::JAILED) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::ALREADY_JAILED,
            "Validator is already jailed."
        );
    }

    if (existing.status() != ValidatorRegistrationStatus::ACTIVE) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_STATUS_TRANSITION,
            "Only ACTIVE validators can be jailed."
        );
    }

    ValidatorRegistryEntry updated(
        existing.registrationRecord(),
        ValidatorRegistrationStatus::JAILED,
        timestamp,
        existing.stakeAmount(),
        jailUntilEpoch,
        existing.exitRequestHeight(),
        existing.ownerAddress()
    );

    it->second = updated;
    return ValidatorRegistryUpdateResult::jailed(updated);
}

ValidatorRegistryUpdateResult ValidatorRegistry::unjailValidator(
    const std::string& validatorAddress,
    std::uint64_t currentEpoch,
    std::int64_t timestamp
) {
    auto it = m_entries.find(validatorAddress);
    if (it == m_entries.end()) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Validator is not registered."
        );
    }

    const ValidatorRegistryEntry& existing = it->second;

    if (existing.status() != ValidatorRegistrationStatus::JAILED) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_STATUS_TRANSITION,
            "Only JAILED validators can be unjailed."
        );
    }

    if (existing.jailUntilEpoch() > currentEpoch) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_STATUS_TRANSITION,
            "Jail period has not elapsed (until epoch "
                + std::to_string(existing.jailUntilEpoch()) + ")."
        );
    }

    ValidatorRegistryEntry updated(
        existing.registrationRecord(),
        ValidatorRegistrationStatus::ACTIVE,
        timestamp,
        existing.stakeAmount(),
        0,
        existing.exitRequestHeight(),
        existing.ownerAddress()
    );

    it->second = updated;
    return ValidatorRegistryUpdateResult::unjailed(updated);
}

ValidatorRegistryUpdateResult ValidatorRegistry::requestExit(
    const std::string& validatorAddress,
    std::uint64_t requestHeight,
    std::int64_t timestamp
) {
    auto it = m_entries.find(validatorAddress);
    if (it == m_entries.end()) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Validator is not registered."
        );
    }

    const ValidatorRegistryEntry& existing = it->second;

    const bool canExit =
        existing.status() == ValidatorRegistrationStatus::ACTIVE ||
        existing.status() == ValidatorRegistrationStatus::JAILED;

    if (!canExit) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_STATUS_TRANSITION,
            "Only ACTIVE or JAILED validators can request exit."
        );
    }

    ValidatorRegistryEntry updated(
        existing.registrationRecord(),
        ValidatorRegistrationStatus::EXIT_REQUESTED,
        timestamp,
        existing.stakeAmount(),
        existing.jailUntilEpoch(),
        requestHeight,
        existing.ownerAddress()
    );

    it->second = updated;
    return ValidatorRegistryUpdateResult::exitRequested(updated);
}

ValidatorRegistryUpdateResult ValidatorRegistry::completeExit(
    const std::string& validatorAddress,
    std::int64_t timestamp
) {
    auto it = m_entries.find(validatorAddress);
    if (it == m_entries.end()) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Validator is not registered."
        );
    }

    const ValidatorRegistryEntry& existing = it->second;

    if (existing.status() != ValidatorRegistrationStatus::EXIT_REQUESTED) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_STATUS_TRANSITION,
            "Only EXIT_REQUESTED validators can complete exit."
        );
    }

    ValidatorRegistryEntry updated(
        existing.registrationRecord(),
        ValidatorRegistrationStatus::EXITED,
        timestamp,
        existing.stakeAmount(),
        existing.jailUntilEpoch(),
        existing.exitRequestHeight(),
        existing.ownerAddress()
    );

    it->second = updated;
    return ValidatorRegistryUpdateResult::deactivated(updated);
}

ValidatorRegistryUpdateResult ValidatorRegistry::updateStake(
    const std::string& validatorAddress,
    std::uint64_t newStakeAmount,
    std::int64_t timestamp
) {
    auto it = m_entries.find(validatorAddress);
    if (it == m_entries.end()) {
        return ValidatorRegistryUpdateResult::rejected(
            ValidatorRegistryUpdateStatus::INVALID_RECORD,
            "Validator is not registered."
        );
    }

    const ValidatorRegistryEntry& existing = it->second;

    ValidatorRegistryEntry updated(
        existing.registrationRecord(),
        existing.status(),
        timestamp,
        newStakeAmount,
        existing.jailUntilEpoch(),
        existing.exitRequestHeight(),
        existing.ownerAddress()
    );

    it->second = updated;
    return ValidatorRegistryUpdateResult::accepted(updated);
}

std::size_t ValidatorRegistry::size() const {
    return m_entries.size();
}

std::size_t ValidatorRegistry::activeCount() const {
    std::size_t count = 0;

    for (const auto& [address, entry] : m_entries) {
        (void)address;

        if (entry.active()) {
            ++count;
        }
    }

    return count;
}

bool ValidatorRegistry::isValid() const {
    for (const auto& [address, entry] : m_entries) {
        if (!isSafeScalar(address)) {
            return false;
        }

        if (!entry.isValid()) {
            return false;
        }

        if (entry.registrationRecord().validatorAddress() != address) {
            return false;
        }
    }

    return true;
}

std::string ValidatorRegistry::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorRegistry{"
        << "size=" << m_entries.size()
        << ";activeCount=" << activeCount()
        << ";entries=[";

    bool first = true;

    for (const auto& [address, entry] : m_entries) {
        (void)address;

        if (!first) {
            oss << ",";
        }

        oss << entry.serialize();
        first = false;
    }

    oss << "]}";

    return oss.str();
}

bool ValidatorSetHistory::recordSet(
    std::uint64_t height,
    const ValidatorRegistry& registry
) {
    if (height == 0 || !registry.isValid()) {
        return false;
    }
    const auto existing = m_setsByHeight.find(height);
    if (existing != m_setsByHeight.end()) {
        return existing->second.serialize() == registry.serialize();
    }
    if (!m_setsByHeight.empty() &&
        m_setsByHeight.rbegin()->first ==
            std::numeric_limits<std::uint64_t>::max()) {
        return false;
    }
    const std::uint64_t expectedHeight =
        m_setsByHeight.empty() ? 1 : m_setsByHeight.rbegin()->first + 1;
    if (height != expectedHeight) {
        return false;
    }
    m_setsByHeight.emplace(height, registry);
    return true;
}

bool ValidatorSetHistory::hasSet(std::uint64_t height) const {
    return m_setsByHeight.find(height) != m_setsByHeight.end();
}

const ValidatorRegistry& ValidatorSetHistory::setAt(std::uint64_t height) const {
    const auto found = m_setsByHeight.find(height);
    if (found == m_setsByHeight.end()) {
        throw std::out_of_range("No validator set recorded for block height.");
    }
    return found->second;
}

std::uint64_t ValidatorSetHistory::highestRecordedHeight() const {
    return m_setsByHeight.empty() ? 0 : m_setsByHeight.rbegin()->first;
}

bool ValidatorSetHistory::isValid() const {
    std::uint64_t expectedHeight = 1;
    for (const auto& [height, registry] : m_setsByHeight) {
        if (height != expectedHeight || !registry.isValid()) {
            return false;
        }
        ++expectedHeight;
    }
    return true;
}

std::string ValidatorSetHistory::serialize() const {
    std::ostringstream output;
    output << "ValidatorSetHistory{size=" << m_setsByHeight.size() << ";sets=[";
    bool first = true;
    for (const auto& [height, registry] : m_setsByHeight) {
        if (!first) output << ",";
        output << "ValidatorSet{height=" << height
               << ";registry=" << registry.serialize() << "}";
        first = false;
    }
    output << "]}";
    return output.str();
}

} // namespace nodo::core
