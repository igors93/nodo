#ifndef NODO_NODE_PROTOCOL_SAFETY_GATE_HPP
#define NODO_NODE_PROTOCOL_SAFETY_GATE_HPP

#include "config/NetworkParameters.hpp"

#include <string>

namespace nodo::node {

enum class ProtocolSafetyStatus {
    ALLOWED,
    REJECTED
};

std::string protocolSafetyStatusToString(
    ProtocolSafetyStatus status
);

class ProtocolSafetyDecision {
public:
    ProtocolSafetyDecision();

    static ProtocolSafetyDecision allowed(
        std::string reason
    );

    static ProtocolSafetyDecision rejected(
        std::string reason
    );

    ProtocolSafetyStatus status() const;
    const std::string& reason() const;
    bool allowed() const;

    std::string serialize() const;

private:
    ProtocolSafetyStatus m_status;
    std::string m_reason;
};

class ProtocolSafetyGate {
public:
    static bool isLocalDevelopmentNetwork(
        const config::NetworkParameters& networkParameters
    );

    static ProtocolSafetyDecision requirePlaintextKeyStoreAllowed(
        const config::NetworkParameters& networkParameters,
        const std::string& keyNetworkProfile
    );

    static ProtocolSafetyDecision requireDeterministicKeyAllowed(
        const config::NetworkParameters& networkParameters,
        const std::string& keyNetworkProfile
    );

    static ProtocolSafetyDecision evaluateLocalKeyCreation(
        const config::NetworkParameters& networkParameters,
        const std::string& keyNetworkProfile,
        bool deterministicSeed,
        bool plaintextPrivateMaterial
    );
};

} // namespace nodo::node

#endif
