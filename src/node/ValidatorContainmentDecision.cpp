#include "node/ValidatorContainmentDecision.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

ValidatorContainmentDecision::ValidatorContainmentDecision()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_riskLevel(""),
      m_recommendedAction(""),
      m_containmentMode(""),
      m_peerTrustState(""),
      m_networkAdmissionState(""),
      m_reason(""),
      m_sourceRiskDigest("") {}

ValidatorContainmentDecision::ValidatorContainmentDecision(
    std::string validatorAddress,
    std::uint64_t blockHeight,
    std::string riskLevel,
    std::string recommendedAction,
    std::string containmentMode,
    std::string peerTrustState,
    std::string networkAdmissionState,
    std::string reason,
    std::string sourceRiskDigest
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_riskLevel(std::move(riskLevel)),
      m_recommendedAction(std::move(recommendedAction)),
      m_containmentMode(std::move(containmentMode)),
      m_peerTrustState(std::move(peerTrustState)),
      m_networkAdmissionState(std::move(networkAdmissionState)),
      m_reason(std::move(reason)),
      m_sourceRiskDigest(std::move(sourceRiskDigest)) {}

const std::string& ValidatorContainmentDecision::validatorAddress() const {
    return m_validatorAddress;
}

std::uint64_t ValidatorContainmentDecision::blockHeight() const {
    return m_blockHeight;
}

const std::string& ValidatorContainmentDecision::riskLevel() const {
    return m_riskLevel;
}

const std::string& ValidatorContainmentDecision::recommendedAction() const {
    return m_recommendedAction;
}

const std::string& ValidatorContainmentDecision::containmentMode() const {
    return m_containmentMode;
}

const std::string& ValidatorContainmentDecision::peerTrustState() const {
    return m_peerTrustState;
}

const std::string& ValidatorContainmentDecision::networkAdmissionState() const {
    return m_networkAdmissionState;
}

const std::string& ValidatorContainmentDecision::reason() const {
    return m_reason;
}

const std::string& ValidatorContainmentDecision::sourceRiskDigest() const {
    return m_sourceRiskDigest;
}

bool ValidatorContainmentDecision::isValid() const {
    if (m_validatorAddress.empty() ||
        m_blockHeight == 0 ||
        m_riskLevel.empty() ||
        m_recommendedAction.empty() ||
        m_containmentMode != ValidatorContainmentDecisionBuilder::containmentModeForRecommendedAction(m_recommendedAction) ||
        m_peerTrustState != ValidatorContainmentDecisionBuilder::peerTrustStateForContainmentMode(m_containmentMode) ||
        m_networkAdmissionState != ValidatorContainmentDecisionBuilder::networkAdmissionStateForContainmentMode(m_containmentMode) ||
        m_reason != ValidatorContainmentDecisionBuilder::VALIDATOR_CONTAINMENT_DECISION_REASON ||
        m_sourceRiskDigest.empty()) {
        return false;
    }

    return true;
}

std::string ValidatorContainmentDecision::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorContainmentDecision{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";riskLevel=" << m_riskLevel
        << ";recommendedAction=" << m_recommendedAction
        << ";containmentMode=" << m_containmentMode
        << ";peerTrustState=" << m_peerTrustState
        << ";networkAdmissionState=" << m_networkAdmissionState
        << ";reason=" << m_reason
        << ";sourceRiskDigest=" << m_sourceRiskDigest
        << "}";

    return oss.str();
}

std::string ValidatorContainmentDecisionBuilder::sourceRiskDigest(
    const ValidatorRiskAssessment& assessment
) {
    if (!assessment.isValid()) {
        throw std::invalid_argument("Cannot build containment source digest from invalid validator risk assessment.");
    }

    std::ostringstream oss;

    oss << "validator-risk:"
        << assessment.blockHeight()
        << ":"
        << assessment.validatorAddress()
        << ":"
        << assessment.riskScore()
        << ":"
        << assessment.riskLevel()
        << ":"
        << assessment.recommendedAction()
        << ":"
        << assessment.checkpointDigest();

    return oss.str();
}

std::string ValidatorContainmentDecisionBuilder::containmentModeForRecommendedAction(
    const std::string& recommendedAction
) {
    if (recommendedAction == "ALLOW") {
        return "NONE";
    }

    if (recommendedAction == "MONITOR") {
        return "OBSERVE";
    }

    if (recommendedAction == "LIMIT_TRUST") {
        return "RESTRICT_TRUST";
    }

    if (recommendedAction == "QUARANTINE_REVIEW") {
        return "REVIEW_QUARANTINE";
    }

    throw std::invalid_argument("Unknown validator containment recommended action.");
}

std::string ValidatorContainmentDecisionBuilder::peerTrustStateForContainmentMode(
    const std::string& containmentMode
) {
    if (containmentMode == "NONE") {
        return "TRUSTED";
    }

    if (containmentMode == "OBSERVE") {
        return "WATCHED";
    }

    if (containmentMode == "RESTRICT_TRUST") {
        return "LIMITED";
    }

    if (containmentMode == "REVIEW_QUARANTINE") {
        return "QUARANTINE_CANDIDATE";
    }

    throw std::invalid_argument("Unknown validator containment mode.");
}

std::string ValidatorContainmentDecisionBuilder::networkAdmissionStateForContainmentMode(
    const std::string& containmentMode
) {
    if (containmentMode == "NONE") {
        return "ADMIT";
    }

    if (containmentMode == "OBSERVE") {
        return "ADMIT_WITH_AUDIT";
    }

    if (containmentMode == "RESTRICT_TRUST") {
        return "ADMIT_LIMITED";
    }

    if (containmentMode == "REVIEW_QUARANTINE") {
        return "REQUIRE_REVIEW";
    }

    throw std::invalid_argument("Unknown validator containment mode.");
}

ValidatorContainmentDecision ValidatorContainmentDecisionBuilder::buildFromRiskAssessment(
    const ValidatorRiskAssessment& assessment
) {
    if (!assessment.isValid()) {
        throw std::invalid_argument("Cannot build containment decision from invalid validator risk assessment.");
    }

    const std::string containmentMode =
        containmentModeForRecommendedAction(
            assessment.recommendedAction()
        );

    return ValidatorContainmentDecision(
        assessment.validatorAddress(),
        assessment.blockHeight(),
        assessment.riskLevel(),
        assessment.recommendedAction(),
        containmentMode,
        peerTrustStateForContainmentMode(containmentMode),
        networkAdmissionStateForContainmentMode(containmentMode),
        VALIDATOR_CONTAINMENT_DECISION_REASON,
        sourceRiskDigest(assessment)
    );
}

std::vector<ValidatorContainmentDecision> ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(
    const std::vector<ValidatorRiskAssessment>& assessments
) {
    std::vector<ValidatorContainmentDecision> decisions;
    decisions.reserve(assessments.size());

    for (const ValidatorRiskAssessment& assessment : assessments) {
        decisions.push_back(
            buildFromRiskAssessment(
                assessment
            )
        );
    }

    return decisions;
}

bool ValidatorContainmentDecisionBuilder::sameDecisions(
    const std::vector<ValidatorContainmentDecision>& left,
    const std::vector<ValidatorContainmentDecision>& right
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
