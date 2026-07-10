#include "node/ProductionKeySafetyGate.hpp"

#include "crypto/KeyEncryptionPolicy.hpp"

#include <utility>

namespace nodo::node {

std::string keySafetyStatusToString(KeySafetyStatus status) {
    switch (status) {
        case KeySafetyStatus::APPROVED:
            return "APPROVED";
        case KeySafetyStatus::REJECTED_PLAINTEXT_ON_OFFICIAL_NETWORK:
            return "REJECTED_PLAINTEXT_ON_OFFICIAL_NETWORK";
        case KeySafetyStatus::REJECTED_MAINNET_NOT_READY:
            return "REJECTED_MAINNET_NOT_READY";
        case KeySafetyStatus::REJECTED_UNKNOWN_PROFILE:
            return "REJECTED_UNKNOWN_PROFILE";
        case KeySafetyStatus::REJECTED_NETWORK_MISMATCH:
            return "REJECTED_NETWORK_MISMATCH";
        case KeySafetyStatus::REJECTED_INSUFFICIENT_ENCRYPTION_LEVEL:
            return "REJECTED_INSUFFICIENT_ENCRYPTION_LEVEL";
        default:
            return "UNKNOWN";
    }
}

KeySafetyCheckResult::KeySafetyCheckResult()
    : m_status(KeySafetyStatus::REJECTED_UNKNOWN_PROFILE),
      m_reason("") {}

KeySafetyCheckResult KeySafetyCheckResult::approved() {
    KeySafetyCheckResult r;
    r.m_status = KeySafetyStatus::APPROVED;
    r.m_reason = "Key policy approved for network.";
    return r;
}

KeySafetyCheckResult KeySafetyCheckResult::rejected(
    KeySafetyStatus status,
    std::string reason
) {
    KeySafetyCheckResult r;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

KeySafetyStatus KeySafetyCheckResult::status() const { return m_status; }
const std::string& KeySafetyCheckResult::reason() const { return m_reason; }
bool KeySafetyCheckResult::isApproved() const { return m_status == KeySafetyStatus::APPROVED; }

KeySafetyCheckResult ProductionKeySafetyGate::check(
    const crypto::StoredKeyMetadata& metadata,
    const std::string& targetNetworkName
) {
    // mainnet is unconditionally blocked until an audited provider exists.
    if (crypto::KeyEncryptionPolicy::isMainnetBlocked(targetNetworkName)) {
        return KeySafetyCheckResult::rejected(
            KeySafetyStatus::REJECTED_MAINNET_NOT_READY,
            "mainnet key usage is blocked: no audited key provider is available. "
            "This will remain blocked until a production HSM/KMS integration is complete."
        );
    }

    // localnet keys (isLocalnetOnly) must not be used on official networks.
    if (metadata.isLocalnetOnly() &&
        crypto::KeyEncryptionPolicy::isOfficialNetwork(targetNetworkName)) {
        return KeySafetyCheckResult::rejected(
            KeySafetyStatus::REJECTED_PLAINTEXT_ON_OFFICIAL_NETWORK,
            "Key '" + metadata.keyId() + "' is a localnet-only plaintext key and "
            "cannot be used on official network '" + targetNetworkName + "'. "
            "Create a network-appropriate key for this profile."
        );
    }

    // Reject any key whose encryption level does not meet the minimum bar
    // required for the target network (e.g. DEV_ENCRYPTED is not sufficient
    // for testnet, which requires TESTNET_SAFE).
    if (crypto::KeyEncryptionPolicy::isOfficialNetwork(targetNetworkName) &&
        !crypto::KeyEncryptionPolicy::isAcceptable(metadata.encryptionLevel(), targetNetworkName)) {
        return KeySafetyCheckResult::rejected(
            KeySafetyStatus::REJECTED_INSUFFICIENT_ENCRYPTION_LEVEL,
            "Key '" + metadata.keyId() + "' is encrypted at level '" +
            crypto::keyEncryptionLevelToString(metadata.encryptionLevel()) +
            "', which does not meet the minimum required level '" +
            crypto::keyEncryptionLevelToString(
                crypto::KeyEncryptionPolicy::requiredLevelForNetwork(targetNetworkName)) +
            "' for official network '" + targetNetworkName + "'."
        );
    }

    // A key bound to a specific network profile must match the target network.
    const std::string& keyProfile = metadata.networkProfile();
    if (!keyProfile.empty() &&
        keyProfile != "localnet" &&
        keyProfile != targetNetworkName) {
        return KeySafetyCheckResult::rejected(
            KeySafetyStatus::REJECTED_NETWORK_MISMATCH,
            "Key '" + metadata.keyId() + "' was created for network '" + keyProfile +
            "' but the node is starting on '" + targetNetworkName + "'."
        );
    }

    return KeySafetyCheckResult::approved();
}

} // namespace nodo::node
