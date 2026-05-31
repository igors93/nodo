#ifndef NODO_NODE_VALIDATOR_SECURITY_POSTURE_HPP
#define NODO_NODE_VALIDATOR_SECURITY_POSTURE_HPP

#include "node/ValidatorNetworkPolicy.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class ValidatorSecurityPosture {
public:
    ValidatorSecurityPosture();

    ValidatorSecurityPosture(
        std::string validatorAddress,
        std::uint64_t blockHeight,
        std::string postureState,
        std::string enforcementReadiness,
        std::string finalDisposition,
        std::string connectionPolicy,
        std::string messagePolicy,
        std::string consensusPolicy,
        bool requiresManualReview,
        std::string reason,
        std::string sourcePolicyDigest
    );

    const std::string& validatorAddress() const;
    std::uint64_t blockHeight() const;
    const std::string& postureState() const;
    const std::string& enforcementReadiness() const;
    const std::string& finalDisposition() const;
    const std::string& connectionPolicy() const;
    const std::string& messagePolicy() const;
    const std::string& consensusPolicy() const;
    bool requiresManualReview() const;
    const std::string& reason() const;
    const std::string& sourcePolicyDigest() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_blockHeight;
    std::string m_postureState;
    std::string m_enforcementReadiness;
    std::string m_finalDisposition;
    std::string m_connectionPolicy;
    std::string m_messagePolicy;
    std::string m_consensusPolicy;
    bool m_requiresManualReview;
    std::string m_reason;
    std::string m_sourcePolicyDigest;
};

class ValidatorSecurityPostureBuilder {
public:
    static constexpr const char* VALIDATOR_SECURITY_POSTURE_REASON =
        "VALIDATOR_SECURITY_POSTURE";

    static std::string sourcePolicyDigest(
        const ValidatorNetworkPolicy& policy
    );

    static std::string postureStateForConnectionPolicy(
        const std::string& connectionPolicy
    );

    static std::string enforcementReadinessForPostureState(
        const std::string& postureState
    );

    static std::string finalDispositionForPostureState(
        const std::string& postureState
    );

    static ValidatorSecurityPosture buildFromNetworkPolicy(
        const ValidatorNetworkPolicy& policy
    );

    static std::vector<ValidatorSecurityPosture> buildFromNetworkPolicies(
        const std::vector<ValidatorNetworkPolicy>& policies
    );

    static bool samePostures(
        const std::vector<ValidatorSecurityPosture>& left,
        const std::vector<ValidatorSecurityPosture>& right
    );
};

} // namespace nodo::node

#endif
