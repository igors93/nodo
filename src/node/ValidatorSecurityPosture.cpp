#include "node/ValidatorSecurityPosture.hpp"

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

ValidatorSecurityPosture::ValidatorSecurityPosture()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_postureState(""),
      m_enforcementReadiness(""),
      m_finalDisposition(""),
      m_connectionPolicy(""),
      m_messagePolicy(""),
      m_consensusPolicy(""),
      m_requiresManualReview(false),
      m_reason(""),
      m_sourcePolicyDigest("") {}

ValidatorSecurityPosture::ValidatorSecurityPosture(
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
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_postureState(std::move(postureState)),
      m_enforcementReadiness(std::move(enforcementReadiness)),
      m_finalDisposition(std::move(finalDisposition)),
      m_connectionPolicy(std::move(connectionPolicy)),
      m_messagePolicy(std::move(messagePolicy)),
      m_consensusPolicy(std::move(consensusPolicy)),
      m_requiresManualReview(requiresManualReview),
      m_reason(std::move(reason)),
      m_sourcePolicyDigest(std::move(sourcePolicyDigest)) {}

const std::string& ValidatorSecurityPosture::validatorAddress() const {
    return m_validatorAddress;
}

std::uint64_t ValidatorSecurityPosture::blockHeight() const {
    return m_blockHeight;
}

const std::string& ValidatorSecurityPosture::postureState() const {
    return m_postureState;
}

const std::string& ValidatorSecurityPosture::enforcementReadiness() const {
    return m_enforcementReadiness;
}

const std::string& ValidatorSecurityPosture::finalDisposition() const {
    return m_finalDisposition;
}

const std::string& ValidatorSecurityPosture::connectionPolicy() const {
    return m_connectionPolicy;
}

const std::string& ValidatorSecurityPosture::messagePolicy() const {
    return m_messagePolicy;
}

const std::string& ValidatorSecurityPosture::consensusPolicy() const {
    return m_consensusPolicy;
}

bool ValidatorSecurityPosture::requiresManualReview() const {
    return m_requiresManualReview;
}

const std::string& ValidatorSecurityPosture::reason() const {
    return m_reason;
}

const std::string& ValidatorSecurityPosture::sourcePolicyDigest() const {
    return m_sourcePolicyDigest;
}

bool ValidatorSecurityPosture::isValid() const {
    if (m_validatorAddress.empty() ||
        m_blockHeight == 0 ||
        m_connectionPolicy.empty() ||
        m_messagePolicy.empty() ||
        m_consensusPolicy.empty() ||
        m_postureState != ValidatorSecurityPostureBuilder::postureStateForConnectionPolicy(m_connectionPolicy) ||
        m_enforcementReadiness != ValidatorSecurityPostureBuilder::enforcementReadinessForPostureState(m_postureState) ||
        m_finalDisposition != ValidatorSecurityPostureBuilder::finalDispositionForPostureState(m_postureState) ||
        m_reason != ValidatorSecurityPostureBuilder::VALIDATOR_SECURITY_POSTURE_REASON ||
        m_sourcePolicyDigest.empty()) {
        return false;
    }

    if (m_connectionPolicy == "FULL_ACCESS") {
        return m_messagePolicy == "ALLOW_ALL" &&
               m_consensusPolicy == "ALLOW_VOTES" &&
               !m_requiresManualReview;
    }

    if (m_connectionPolicy == "AUDITED_ACCESS") {
        return m_messagePolicy == "ALLOW_WITH_AUDIT" &&
               m_consensusPolicy == "ALLOW_VOTES_WITH_AUDIT" &&
               !m_requiresManualReview;
    }

    if (m_connectionPolicy == "LIMITED_ACCESS") {
        return m_messagePolicy == "RATE_LIMIT_AND_AUDIT" &&
               m_consensusPolicy == "REQUIRE_EXTRA_AUDIT" &&
               !m_requiresManualReview;
    }

    if (m_connectionPolicy == "MANUAL_REVIEW_ONLY") {
        return m_messagePolicy == "BLOCK_UNTIL_REVIEW" &&
               m_consensusPolicy == "HOLD_FOR_REVIEW" &&
               m_requiresManualReview;
    }

    return false;
}

std::string ValidatorSecurityPosture::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorSecurityPosture{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";postureState=" << m_postureState
        << ";enforcementReadiness=" << m_enforcementReadiness
        << ";finalDisposition=" << m_finalDisposition
        << ";connectionPolicy=" << m_connectionPolicy
        << ";messagePolicy=" << m_messagePolicy
        << ";consensusPolicy=" << m_consensusPolicy
        << ";requiresManualReview=" << boolToCanonicalString(m_requiresManualReview)
        << ";reason=" << m_reason
        << ";sourcePolicyDigest=" << m_sourcePolicyDigest
        << "}";

    return oss.str();
}

std::string ValidatorSecurityPostureBuilder::sourcePolicyDigest(
    const ValidatorNetworkPolicy& policy
) {
    if (!policy.isValid()) {
        throw std::invalid_argument("Cannot build security posture source digest from invalid network policy.");
    }

    std::ostringstream oss;

    oss << "validator-network-policy:"
        << policy.blockHeight()
        << ":"
        << policy.validatorAddress()
        << ":"
        << policy.connectionPolicy()
        << ":"
        << policy.messagePolicy()
        << ":"
        << policy.consensusPolicy()
        << ":"
        << (policy.requiresManualReview() ? "true" : "false")
        << ":"
        << policy.sourceContainmentDigest();

    return oss.str();
}

std::string ValidatorSecurityPostureBuilder::postureStateForConnectionPolicy(
    const std::string& connectionPolicy
) {
    if (connectionPolicy == "FULL_ACCESS") {
        return "HEALTHY";
    }

    if (connectionPolicy == "AUDITED_ACCESS") {
        return "OBSERVED";
    }

    if (connectionPolicy == "LIMITED_ACCESS") {
        return "CONSTRAINED";
    }

    if (connectionPolicy == "MANUAL_REVIEW_ONLY") {
        return "CONTAINMENT_READY";
    }

    throw std::invalid_argument("Unknown validator connection policy.");
}

std::string ValidatorSecurityPostureBuilder::enforcementReadinessForPostureState(
    const std::string& postureState
) {
    if (postureState == "HEALTHY") {
        return "ENFORCEMENT_NOT_REQUIRED";
    }

    if (postureState == "OBSERVED") {
        return "AUDIT_READY";
    }

    if (postureState == "CONSTRAINED") {
        return "LIMITED_ENFORCEMENT_READY";
    }

    if (postureState == "CONTAINMENT_READY") {
        return "MANUAL_REVIEW_REQUIRED";
    }

    throw std::invalid_argument("Unknown validator posture state.");
}

std::string ValidatorSecurityPostureBuilder::finalDispositionForPostureState(
    const std::string& postureState
) {
    if (postureState == "HEALTHY") {
        return "NO_ACTION";
    }

    if (postureState == "OBSERVED") {
        return "AUDIT_ONLY";
    }

    if (postureState == "CONSTRAINED") {
        return "RESTRICTIVE_CONTROL_READY";
    }

    if (postureState == "CONTAINMENT_READY") {
        return "QUARANTINE_REVIEW_REQUIRED";
    }

    throw std::invalid_argument("Unknown validator posture state.");
}

ValidatorSecurityPosture ValidatorSecurityPostureBuilder::buildFromNetworkPolicy(
    const ValidatorNetworkPolicy& policy
) {
    if (!policy.isValid()) {
        throw std::invalid_argument("Cannot build security posture from invalid network policy.");
    }

    const std::string postureState =
        postureStateForConnectionPolicy(
            policy.connectionPolicy()
        );

    return ValidatorSecurityPosture(
        policy.validatorAddress(),
        policy.blockHeight(),
        postureState,
        enforcementReadinessForPostureState(postureState),
        finalDispositionForPostureState(postureState),
        policy.connectionPolicy(),
        policy.messagePolicy(),
        policy.consensusPolicy(),
        policy.requiresManualReview(),
        VALIDATOR_SECURITY_POSTURE_REASON,
        sourcePolicyDigest(policy)
    );
}

std::vector<ValidatorSecurityPosture> ValidatorSecurityPostureBuilder::buildFromNetworkPolicies(
    const std::vector<ValidatorNetworkPolicy>& policies
) {
    std::vector<ValidatorSecurityPosture> postures;
    postures.reserve(policies.size());

    for (const ValidatorNetworkPolicy& policy : policies) {
        postures.push_back(
            buildFromNetworkPolicy(
                policy
            )
        );
    }

    return postures;
}

bool ValidatorSecurityPostureBuilder::samePostures(
    const std::vector<ValidatorSecurityPosture>& left,
    const std::vector<ValidatorSecurityPosture>& right
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
