#include "node/ValidatorNetworkPolicy.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::string boolToCanonicalString(
    bool value
) {
    return value ? "true" : "false";
}

} // namespace

ValidatorNetworkPolicy::ValidatorNetworkPolicy()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_containmentMode(""),
      m_peerTrustState(""),
      m_networkAdmissionState(""),
      m_connectionPolicy(""),
      m_messagePolicy(""),
      m_consensusPolicy(""),
      m_requiresManualReview(false),
      m_reason(""),
      m_sourceContainmentDigest("") {}

ValidatorNetworkPolicy::ValidatorNetworkPolicy(
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
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_containmentMode(std::move(containmentMode)),
      m_peerTrustState(std::move(peerTrustState)),
      m_networkAdmissionState(std::move(networkAdmissionState)),
      m_connectionPolicy(std::move(connectionPolicy)),
      m_messagePolicy(std::move(messagePolicy)),
      m_consensusPolicy(std::move(consensusPolicy)),
      m_requiresManualReview(requiresManualReview),
      m_reason(std::move(reason)),
      m_sourceContainmentDigest(std::move(sourceContainmentDigest)) {}

const std::string& ValidatorNetworkPolicy::validatorAddress() const {
    return m_validatorAddress;
}

std::uint64_t ValidatorNetworkPolicy::blockHeight() const {
    return m_blockHeight;
}

const std::string& ValidatorNetworkPolicy::containmentMode() const {
    return m_containmentMode;
}

const std::string& ValidatorNetworkPolicy::peerTrustState() const {
    return m_peerTrustState;
}

const std::string& ValidatorNetworkPolicy::networkAdmissionState() const {
    return m_networkAdmissionState;
}

const std::string& ValidatorNetworkPolicy::connectionPolicy() const {
    return m_connectionPolicy;
}

const std::string& ValidatorNetworkPolicy::messagePolicy() const {
    return m_messagePolicy;
}

const std::string& ValidatorNetworkPolicy::consensusPolicy() const {
    return m_consensusPolicy;
}

bool ValidatorNetworkPolicy::requiresManualReview() const {
    return m_requiresManualReview;
}

const std::string& ValidatorNetworkPolicy::reason() const {
    return m_reason;
}

const std::string& ValidatorNetworkPolicy::sourceContainmentDigest() const {
    return m_sourceContainmentDigest;
}

bool ValidatorNetworkPolicy::isValid() const {
    if (m_validatorAddress.empty() ||
        m_blockHeight == 0 ||
        m_containmentMode.empty() ||
        m_peerTrustState != ValidatorContainmentDecisionBuilder::peerTrustStateForContainmentMode(m_containmentMode) ||
        m_networkAdmissionState != ValidatorContainmentDecisionBuilder::networkAdmissionStateForContainmentMode(m_containmentMode) ||
        m_connectionPolicy != ValidatorNetworkPolicyBuilder::connectionPolicyForContainmentMode(m_containmentMode) ||
        m_messagePolicy != ValidatorNetworkPolicyBuilder::messagePolicyForContainmentMode(m_containmentMode) ||
        m_consensusPolicy != ValidatorNetworkPolicyBuilder::consensusPolicyForContainmentMode(m_containmentMode) ||
        m_requiresManualReview != ValidatorNetworkPolicyBuilder::requiresManualReviewForContainmentMode(m_containmentMode) ||
        m_reason != ValidatorNetworkPolicyBuilder::VALIDATOR_NETWORK_POLICY_REASON ||
        m_sourceContainmentDigest.empty()) {
        return false;
    }

    return true;
}

std::string ValidatorNetworkPolicy::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorNetworkPolicy{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";containmentMode=" << m_containmentMode
        << ";peerTrustState=" << m_peerTrustState
        << ";networkAdmissionState=" << m_networkAdmissionState
        << ";connectionPolicy=" << m_connectionPolicy
        << ";messagePolicy=" << m_messagePolicy
        << ";consensusPolicy=" << m_consensusPolicy
        << ";requiresManualReview=" << boolToCanonicalString(m_requiresManualReview)
        << ";reason=" << m_reason
        << ";sourceContainmentDigest=" << m_sourceContainmentDigest
        << "}";

    return oss.str();
}

std::string ValidatorNetworkPolicyBuilder::sourceContainmentDigest(
    const ValidatorContainmentDecision& decision
) {
    if (!decision.isValid()) {
        throw std::invalid_argument("Cannot build network policy source digest from invalid containment decision.");
    }

    std::ostringstream oss;

    oss << "validator-containment:"
        << decision.blockHeight()
        << ":"
        << decision.validatorAddress()
        << ":"
        << decision.containmentMode()
        << ":"
        << decision.peerTrustState()
        << ":"
        << decision.networkAdmissionState()
        << ":"
        << decision.sourceRiskDigest();

    return oss.str();
}

std::string ValidatorNetworkPolicyBuilder::connectionPolicyForContainmentMode(
    const std::string& containmentMode
) {
    if (containmentMode == "NONE") {
        return "FULL_ACCESS";
    }

    if (containmentMode == "OBSERVE") {
        return "AUDITED_ACCESS";
    }

    if (containmentMode == "RESTRICT_TRUST") {
        return "LIMITED_ACCESS";
    }

    if (containmentMode == "REVIEW_QUARANTINE") {
        return "MANUAL_REVIEW_ONLY";
    }

    throw std::invalid_argument("Unknown validator containment mode.");
}

std::string ValidatorNetworkPolicyBuilder::messagePolicyForContainmentMode(
    const std::string& containmentMode
) {
    if (containmentMode == "NONE") {
        return "ALLOW_ALL";
    }

    if (containmentMode == "OBSERVE") {
        return "ALLOW_WITH_AUDIT";
    }

    if (containmentMode == "RESTRICT_TRUST") {
        return "RATE_LIMIT_AND_AUDIT";
    }

    if (containmentMode == "REVIEW_QUARANTINE") {
        return "BLOCK_UNTIL_REVIEW";
    }

    throw std::invalid_argument("Unknown validator containment mode.");
}

std::string ValidatorNetworkPolicyBuilder::consensusPolicyForContainmentMode(
    const std::string& containmentMode
) {
    if (containmentMode == "NONE") {
        return "ALLOW_VOTES";
    }

    if (containmentMode == "OBSERVE") {
        return "ALLOW_VOTES_WITH_AUDIT";
    }

    if (containmentMode == "RESTRICT_TRUST") {
        return "REQUIRE_EXTRA_AUDIT";
    }

    if (containmentMode == "REVIEW_QUARANTINE") {
        return "HOLD_FOR_REVIEW";
    }

    throw std::invalid_argument("Unknown validator containment mode.");
}

bool ValidatorNetworkPolicyBuilder::requiresManualReviewForContainmentMode(
    const std::string& containmentMode
) {
    if (containmentMode == "NONE" ||
        containmentMode == "OBSERVE" ||
        containmentMode == "RESTRICT_TRUST") {
        return false;
    }

    if (containmentMode == "REVIEW_QUARANTINE") {
        return true;
    }

    throw std::invalid_argument("Unknown validator containment mode.");
}

ValidatorNetworkPolicy ValidatorNetworkPolicyBuilder::buildFromContainmentDecision(
    const ValidatorContainmentDecision& decision
) {
    if (!decision.isValid()) {
        throw std::invalid_argument("Cannot build network policy from invalid containment decision.");
    }

    return ValidatorNetworkPolicy(
        decision.validatorAddress(),
        decision.blockHeight(),
        decision.containmentMode(),
        decision.peerTrustState(),
        decision.networkAdmissionState(),
        connectionPolicyForContainmentMode(decision.containmentMode()),
        messagePolicyForContainmentMode(decision.containmentMode()),
        consensusPolicyForContainmentMode(decision.containmentMode()),
        requiresManualReviewForContainmentMode(decision.containmentMode()),
        VALIDATOR_NETWORK_POLICY_REASON,
        sourceContainmentDigest(decision)
    );
}

std::vector<ValidatorNetworkPolicy> ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(
    const std::vector<ValidatorContainmentDecision>& decisions
) {
    std::vector<ValidatorNetworkPolicy> policies;
    policies.reserve(decisions.size());

    for (const ValidatorContainmentDecision& decision : decisions) {
        policies.push_back(
            buildFromContainmentDecision(
                decision
            )
        );
    }

    return policies;
}

bool ValidatorNetworkPolicyBuilder::samePolicies(
    const std::vector<ValidatorNetworkPolicy>& left,
    const std::vector<ValidatorNetworkPolicy>& right
) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].serialize() != right[index].serialize()) {
            return false;
        }
    }

    return true;
}

} // namespace nodo::node
