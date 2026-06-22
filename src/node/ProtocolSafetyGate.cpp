#include "node/ProtocolSafetyGate.hpp"

#include "crypto/KeyStore.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

std::string protocolSafetyStatusToString(
    ProtocolSafetyStatus status
) {
    switch (status) {
        case ProtocolSafetyStatus::ALLOWED:
            return "ALLOWED";
        case ProtocolSafetyStatus::REJECTED:
            return "REJECTED";
        default:
            return "REJECTED";
    }
}

ProtocolSafetyDecision::ProtocolSafetyDecision()
    : m_status(ProtocolSafetyStatus::REJECTED),
      m_reason("Uninitialized protocol safety decision.") {}

ProtocolSafetyDecision ProtocolSafetyDecision::allowed(
    std::string reason
) {
    ProtocolSafetyDecision decision;
    decision.m_status = ProtocolSafetyStatus::ALLOWED;
    decision.m_reason = std::move(reason);
    return decision;
}

ProtocolSafetyDecision ProtocolSafetyDecision::rejected(
    std::string reason
) {
    ProtocolSafetyDecision decision;
    decision.m_status = ProtocolSafetyStatus::REJECTED;
    decision.m_reason = std::move(reason);
    return decision;
}

ProtocolSafetyStatus ProtocolSafetyDecision::status() const {
    return m_status;
}

const std::string& ProtocolSafetyDecision::reason() const {
    return m_reason;
}

bool ProtocolSafetyDecision::allowed() const {
    return m_status == ProtocolSafetyStatus::ALLOWED;
}

std::string ProtocolSafetyDecision::serialize() const {
    std::ostringstream output;

    output << "ProtocolSafetyDecision{"
           << "status=" << protocolSafetyStatusToString(m_status)
           << ";reason=" << m_reason
           << "}";

    return output.str();
}

bool ProtocolSafetyGate::isLocalDevelopmentNetwork(
    const config::NetworkParameters& networkParameters
) {
    return networkParameters.isValid() &&
           networkParameters.networkName() == "localnet" &&
           networkParameters.chainId().find("localnet") != std::string::npos;
}

ProtocolSafetyDecision ProtocolSafetyGate::requirePlaintextKeyStoreAllowed(
    const config::NetworkParameters& networkParameters,
    const std::string& keyNetworkProfile
) {
    if (!networkParameters.isValid()) {
        return ProtocolSafetyDecision::rejected(
            "Network parameters are invalid."
        );
    }

    if (isLocalDevelopmentNetwork(networkParameters) &&
        keyNetworkProfile == crypto::KeyStore::LOCAL_NETWORK_PROFILE) {
        return ProtocolSafetyDecision::allowed(
            "Plaintext key material is allowed only for local development."
        );
    }

    return ProtocolSafetyDecision::rejected(
        "Plaintext private key material is rejected outside localnet."
    );
}

ProtocolSafetyDecision ProtocolSafetyGate::requireDeterministicKeyAllowed(
    const config::NetworkParameters& networkParameters,
    const std::string& keyNetworkProfile
) {
    if (!networkParameters.isValid()) {
        return ProtocolSafetyDecision::rejected(
            "Network parameters are invalid."
        );
    }

    if (isLocalDevelopmentNetwork(networkParameters) &&
        keyNetworkProfile == crypto::KeyStore::LOCAL_NETWORK_PROFILE) {
        return ProtocolSafetyDecision::allowed(
            "Deterministic key derivation is allowed only for local development."
        );
    }

    return ProtocolSafetyDecision::rejected(
        "Deterministic key derivation is rejected outside localnet."
    );
}

ProtocolSafetyDecision ProtocolSafetyGate::evaluateLocalKeyCreation(
    const config::NetworkParameters& networkParameters,
    const std::string& keyNetworkProfile,
    bool deterministicSeed,
    bool plaintextPrivateMaterial
) {
    if (deterministicSeed) {
        const ProtocolSafetyDecision deterministic =
            requireDeterministicKeyAllowed(
                networkParameters,
                keyNetworkProfile
            );

        if (!deterministic.allowed()) {
            return deterministic;
        }
    }

    if (plaintextPrivateMaterial) {
        const ProtocolSafetyDecision plaintext =
            requirePlaintextKeyStoreAllowed(
                networkParameters,
                keyNetworkProfile
            );

        if (!plaintext.allowed()) {
            return plaintext;
        }
    }

    return ProtocolSafetyDecision::allowed(
        "Protocol safety gate accepted local key creation policy."
    );
}

} // namespace nodo::node
