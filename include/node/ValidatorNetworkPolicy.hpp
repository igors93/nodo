#ifndef NODO_NODE_VALIDATOR_NETWORK_POLICY_HPP
#define NODO_NODE_VALIDATOR_NETWORK_POLICY_HPP

#include "node/ValidatorContainmentDecision.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class ValidatorNetworkPolicy {
public:
    ValidatorNetworkPolicy();

    ValidatorNetworkPolicy(
        std::string validatorAddress,
        std::uint64_t blockHeight,
        std::string containmentMode,
        std::string peerTrustState,
        std::string networkAdmissionState,
        std::string connectionPolicy,
        std::string messagePolicy,
        std::string consensusPolicy,
        bool requiresManualReview,
        std::string reason,
        std::string sourceContainmentDigest
    );

    const std::string& validatorAddress() const;
    std::uint64_t blockHeight() const;
    const std::string& containmentMode() const;
    const std::string& peerTrustState() const;
    const std::string& networkAdmissionState() const;
    const std::string& connectionPolicy() const;
    const std::string& messagePolicy() const;
    const std::string& consensusPolicy() const;
    bool requiresManualReview() const;
    const std::string& reason() const;
    const std::string& sourceContainmentDigest() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_blockHeight;
    std::string m_containmentMode;
    std::string m_peerTrustState;
    std::string m_networkAdmissionState;
    std::string m_connectionPolicy;
    std::string m_messagePolicy;
    std::string m_consensusPolicy;
    bool m_requiresManualReview;
    std::string m_reason;
    std::string m_sourceContainmentDigest;
};

class ValidatorNetworkPolicyBuilder {
public:
    static constexpr const char* VALIDATOR_NETWORK_POLICY_REASON =
        "VALIDATOR_NETWORK_POLICY";

    static std::string sourceContainmentDigest(
        const ValidatorContainmentDecision& decision
    );

    static std::string connectionPolicyForContainmentMode(
        const std::string& containmentMode
    );

    static std::string messagePolicyForContainmentMode(
        const std::string& containmentMode
    );

    static std::string consensusPolicyForContainmentMode(
        const std::string& containmentMode
    );

    static bool requiresManualReviewForContainmentMode(
        const std::string& containmentMode
    );

    static ValidatorNetworkPolicy buildFromContainmentDecision(
        const ValidatorContainmentDecision& decision
    );

    static std::vector<ValidatorNetworkPolicy> buildFromContainmentDecisions(
        const std::vector<ValidatorContainmentDecision>& decisions
    );

    static bool samePolicies(
        const std::vector<ValidatorNetworkPolicy>& left,
        const std::vector<ValidatorNetworkPolicy>& right
    );
};

} // namespace nodo::node

#endif
