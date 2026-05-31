#ifndef NODO_NODE_VALIDATOR_CONTAINMENT_DECISION_HPP
#define NODO_NODE_VALIDATOR_CONTAINMENT_DECISION_HPP

#include "node/ValidatorRiskAssessment.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class ValidatorContainmentDecision {
public:
    ValidatorContainmentDecision();

    ValidatorContainmentDecision(
        std::string validatorAddress,
        std::uint64_t blockHeight,
        std::string riskLevel,
        std::string recommendedAction,
        std::string containmentMode,
        std::string peerTrustState,
        std::string networkAdmissionState,
        std::string reason,
        std::string sourceRiskDigest
    );

    const std::string& validatorAddress() const;
    std::uint64_t blockHeight() const;
    const std::string& riskLevel() const;
    const std::string& recommendedAction() const;
    const std::string& containmentMode() const;
    const std::string& peerTrustState() const;
    const std::string& networkAdmissionState() const;
    const std::string& reason() const;
    const std::string& sourceRiskDigest() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_blockHeight;
    std::string m_riskLevel;
    std::string m_recommendedAction;
    std::string m_containmentMode;
    std::string m_peerTrustState;
    std::string m_networkAdmissionState;
    std::string m_reason;
    std::string m_sourceRiskDigest;
};

class ValidatorContainmentDecisionBuilder {
public:
    static constexpr const char* VALIDATOR_CONTAINMENT_DECISION_REASON =
        "VALIDATOR_CONTAINMENT_DECISION";

    static std::string sourceRiskDigest(
        const ValidatorRiskAssessment& assessment
    );

    static std::string containmentModeForRecommendedAction(
        const std::string& recommendedAction
    );

    static std::string peerTrustStateForContainmentMode(
        const std::string& containmentMode
    );

    static std::string networkAdmissionStateForContainmentMode(
        const std::string& containmentMode
    );

    static ValidatorContainmentDecision buildFromRiskAssessment(
        const ValidatorRiskAssessment& assessment
    );

    static std::vector<ValidatorContainmentDecision> buildFromRiskAssessments(
        const std::vector<ValidatorRiskAssessment>& assessments
    );

    static bool sameDecisions(
        const std::vector<ValidatorContainmentDecision>& left,
        const std::vector<ValidatorContainmentDecision>& right
    );
};

} // namespace nodo::node

#endif
