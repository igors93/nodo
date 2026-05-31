#ifndef NODO_NODE_VALIDATOR_RISK_ASSESSMENT_HPP
#define NODO_NODE_VALIDATOR_RISK_ASSESSMENT_HPP

#include "node/SecurityCheckpoint.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class ValidatorRiskAssessment {
public:
    ValidatorRiskAssessment();

    ValidatorRiskAssessment(
        std::string validatorAddress,
        std::uint64_t blockHeight,
        std::uint16_t score,
        std::string band,
        utils::Amount lockedStake,
        std::uint16_t riskScore,
        std::string riskLevel,
        std::string recommendedAction,
        std::string reason,
        std::string checkpointDigest
    );

    const std::string& validatorAddress() const;
    std::uint64_t blockHeight() const;
    std::uint16_t score() const;
    const std::string& band() const;
    utils::Amount lockedStake() const;
    std::uint16_t riskScore() const;
    const std::string& riskLevel() const;
    const std::string& recommendedAction() const;
    const std::string& reason() const;
    const std::string& checkpointDigest() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_blockHeight;
    std::uint16_t m_score;
    std::string m_band;
    utils::Amount m_lockedStake;
    std::uint16_t m_riskScore;
    std::string m_riskLevel;
    std::string m_recommendedAction;
    std::string m_reason;
    std::string m_checkpointDigest;
};

class ValidatorRiskAssessmentBuilder {
public:
    static constexpr const char* VALIDATOR_RISK_ASSESSMENT_REASON =
        "VALIDATOR_SECURITY_RISK_ASSESSMENT";

    static std::uint16_t riskScoreForCheckpoint(
        const ValidatorSecurityCheckpoint& checkpoint
    );

    static std::string riskLevelForScore(
        std::uint16_t riskScore
    );

    static std::string recommendedActionForRiskLevel(
        const std::string& riskLevel
    );

    static ValidatorRiskAssessment buildFromCheckpoint(
        const ValidatorSecurityCheckpoint& checkpoint
    );

    static std::vector<ValidatorRiskAssessment> buildFromCheckpoints(
        const std::vector<ValidatorSecurityCheckpoint>& checkpoints
    );

    static bool sameAssessments(
        const std::vector<ValidatorRiskAssessment>& left,
        const std::vector<ValidatorRiskAssessment>& right
    );
};

} // namespace nodo::node

#endif
