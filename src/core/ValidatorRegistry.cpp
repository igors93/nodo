#include "core/ValidatorRegistry.hpp"

#include "crypto/AddressDerivation.hpp"
#include "crypto/hash.h"

#include <sstream>
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
        case ValidatorRegistrationStatus::ACTIVE:
            return "ACTIVE";
        case ValidatorRegistrationStatus::DEACTIVATED:
            return "DEACTIVATED";
        case ValidatorRegistrationStatus::UNKNOWN:
        default:
            return "UNKNOWN";
    }
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
      m_lastUpdatedAt(0) {}

ValidatorRegistryEntry::ValidatorRegistryEntry(
    ValidatorRegistrationRecord registrationRecord,
    ValidatorRegistrationStatus status,
    std::int64_t lastUpdatedAt
)
    : m_registrationRecord(std::move(registrationRecord)),
      m_status(status),
      m_lastUpdatedAt(lastUpdatedAt) {}

const ValidatorRegistrationRecord& ValidatorRegistryEntry::registrationRecord() const {
    return m_registrationRecord;
}

ValidatorRegistrationStatus ValidatorRegistryEntry::status() const {
    return m_status;
}

std::int64_t ValidatorRegistryEntry::lastUpdatedAt() const {
    return m_lastUpdatedAt;
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
        << ";registration=" << m_registrationRecord.serialize()
        << "}";

    return oss.str();
}

std::string validatorRegistryUpdateStatusToString(
    ValidatorRegistryUpdateStatus status
) {
    switch (status) {
        case ValidatorRegistryUpdateStatus::ACCEPTED:
            return "ACCEPTED";
        case ValidatorRegistryUpdateStatus::DUPLICATE:
            return "DUPLICATE";
        case ValidatorRegistryUpdateStatus::DEACTIVATED:
            return "DEACTIVATED";
        case ValidatorRegistryUpdateStatus::CONFLICTING_PUBLIC_KEY:
            return "CONFLICTING_PUBLIC_KEY";
        case ValidatorRegistryUpdateStatus::INVALID_RECORD:
            return "INVALID_RECORD";
        case ValidatorRegistryUpdateStatus::INVALID_REGISTRY:
            return "INVALID_REGISTRY";
        default:
            return "INVALID_REGISTRY";
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
    return accepted() || duplicate() || deactivated();
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

} // namespace nodo::core
