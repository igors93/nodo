#ifndef NODO_NODE_PRODUCTION_KEY_SAFETY_GATE_HPP
#define NODO_NODE_PRODUCTION_KEY_SAFETY_GATE_HPP

#include "crypto/KeyStore.hpp"

#include <string>

namespace nodo::node {

/*
 * KeySafetyStatus describes the outcome of a key policy check.
 *
 * APPROVED means the key may be used for the given network.
 * All REJECTED_ variants carry a reason string explaining what failed.
 */
enum class KeySafetyStatus {
    APPROVED,
    REJECTED_PLAINTEXT_ON_OFFICIAL_NETWORK,
    REJECTED_MAINNET_NOT_READY,
    REJECTED_UNKNOWN_PROFILE,
    REJECTED_NETWORK_MISMATCH,
    REJECTED_INSUFFICIENT_ENCRYPTION_LEVEL
};

std::string keySafetyStatusToString(KeySafetyStatus status);

class KeySafetyCheckResult {
public:
    KeySafetyCheckResult();

    static KeySafetyCheckResult approved();
    static KeySafetyCheckResult rejected(KeySafetyStatus status, std::string reason);

    KeySafetyStatus status() const;
    const std::string& reason() const;
    bool isApproved() const;

private:
    KeySafetyStatus m_status;
    std::string m_reason;
};

/*
 * ProductionKeySafetyGate enforces key usage policy at node startup.
 *
 * Security principle:
 * A node must not start on an official network (testnet, mainnet) using a key
 * that was created for localnet (plaintext, development-only). This gate is
 * called before any peer connection is attempted so that a misconfigured node
 * fails loudly instead of silently degrading network security.
 *
 * mainnet is intentionally blocked: no audited key provider exists yet.
 * This will remain blocked until a proper HSM/KMS integration is complete.
 */
class ProductionKeySafetyGate {
public:
    static KeySafetyCheckResult check(
        const crypto::StoredKeyMetadata& metadata,
        const std::string& targetNetworkName
    );
};

} // namespace nodo::node

#endif
