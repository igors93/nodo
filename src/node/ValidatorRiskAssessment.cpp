#include "node/ValidatorRiskAssessment.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

ValidatorRiskAssessment::ValidatorRiskAssessment()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_score(0),
      m_band(""),
      m_lockedStake(),
      m_riskScore(0),
      m_riskLevel(""),
      m_recommendedAction(""),
      m_reason(""),
      m_checkpointDigest("") {}

ValidatorRiskAssessment::ValidatorRiskAssessment(
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
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_score(score),
      m_band(std::move(band)),
      m_lockedStake(lockedStake),
      m_riskScore(riskScore),
      m_riskLevel(std::move(riskLevel)),
      m_recommendedAction(std::move(recommendedAction)),
      m_reason(std::move(reason)),
      m_checkpointDigest(std::move(checkpointDigest)) {}

const std::string& ValidatorRiskAssessment::validatorAddress() const {
    return m_validatorAddress;
}

std::uint64_t ValidatorRiskAssessment::blockHeight() const {
    return m_blockHeight;
}

std::uint16_t ValidatorRiskAssessment::score() const {
    return m_score;
}

const std::string& ValidatorRiskAssessment::band() const {
    return m_band;
}

utils::Amount ValidatorRiskAssessment::lockedStake() const {
    return m_lockedStake;
}

std::uint16_t ValidatorRiskAssessment::riskScore() const {
    return m_riskScore;
}

const std::string& ValidatorRiskAssessment::riskLevel() const {
    return m_riskLevel;
}

const std::string& ValidatorRiskAssessment::recommendedAction() const {
    return m_recommendedAction;
}

const std::string& ValidatorRiskAssessment::reason() const {
    return m_reason;
}

const std::string& ValidatorRiskAssessment::checkpointDigest() const {
    return m_checkpointDigest;
}

bool ValidatorRiskAssessment::isValid() const {
    if (m_validatorAddress.empty() ||
        m_blockHeight == 0 ||
        m_score < SECURITY_SCORE_MIN ||
        m_score > SECURITY_SCORE_MAX ||
        m_band != ValidatorSecurityCheckpointBuilder::bandForScore(m_score) ||
        m_lockedStake.isNegative() ||
        m_riskScore > 999 ||
        m_riskLevel != ValidatorRiskAssessmentBuilder::riskLevelForScore(m_riskScore) ||
        m_recommendedAction != ValidatorRiskAssessmentBuilder::recommendedActionForRiskLevel(m_riskLevel) ||
        m_reason != ValidatorRiskAssessmentBuilder::VALIDATOR_RISK_ASSESSMENT_REASON ||
        m_checkpointDigest.empty()) {
        return false;
    }

    return true;
}

std::string ValidatorRiskAssessment::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorRiskAssessment{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";score=" << m_score
        << ";band=" << m_band
        << ";lockedStakeRawUnits=" << m_lockedStake.rawUnits()
        << ";riskScore=" << m_riskScore
        << ";riskLevel=" << m_riskLevel
        << ";recommendedAction=" << m_recommendedAction
        << ";reason=" << m_reason
        << ";checkpointDigest=" << m_checkpointDigest
        << "}";

    return oss.str();
}

std::uint16_t ValidatorRiskAssessmentBuilder::riskScoreForCheckpoint(
    const ValidatorSecurityCheckpoint& checkpoint
) {
    if (!checkpoint.isValid()) {
        throw std::invalid_argument("Cannot assess risk from invalid security checkpoint.");
    }

    return static_cast<std::uint16_t>(
        SECURITY_SCORE_MAX - checkpoint.score()
    );
}

std::string ValidatorRiskAssessmentBuilder::riskLevelForScore(
    std::uint16_t riskScore
) {
    if (riskScore >= 900) {
        return "HIGH";
    }

    if (riskScore >= 600) {
        return "ELEVATED";
    }

    if (riskScore >= 300) {
        return "MODERATE";
    }

    return "LOW";
}

std::string ValidatorRiskAssessmentBuilder::recommendedActionForRiskLevel(
    const std::string& riskLevel
) {
    if (riskLevel == "HIGH") {
        return "QUARANTINE_REVIEW";
    }

    if (riskLevel == "ELEVATED") {
        return "LIMIT_TRUST";
    }

    if (riskLevel == "MODERATE") {
        return "MONITOR";
    }

    if (riskLevel == "LOW") {
        return "ALLOW";
    }

    throw std::invalid_argument("Unknown validator risk level.");
}

ValidatorRiskAssessment ValidatorRiskAssessmentBuilder::buildFromCheckpoint(
    const ValidatorSecurityCheckpoint& checkpoint
) {
    if (!checkpoint.isValid()) {
        throw std::invalid_argument("Cannot build risk assessment from invalid security checkpoint.");
    }

    const std::uint16_t riskScore =
        riskScoreForCheckpoint(checkpoint);

    const std::string riskLevel =
        riskLevelForScore(riskScore);

    return ValidatorRiskAssessment(
        checkpoint.validatorAddress(),
        checkpoint.blockHeight(),
        checkpoint.score(),
        checkpoint.band(),
        checkpoint.lockedStake(),
        riskScore,
        riskLevel,
        recommendedActionForRiskLevel(riskLevel),
        VALIDATOR_RISK_ASSESSMENT_REASON,
        checkpoint.sourceDigest()
    );
}

std::vector<ValidatorRiskAssessment> ValidatorRiskAssessmentBuilder::buildFromCheckpoints(
    const std::vector<ValidatorSecurityCheckpoint>& checkpoints
) {
    std::vector<ValidatorRiskAssessment> assessments;
    assessments.reserve(checkpoints.size());

    for (const ValidatorSecurityCheckpoint& checkpoint : checkpoints) {
        assessments.push_back(
            buildFromCheckpoint(checkpoint)
        );
    }

    return assessments;
}

bool ValidatorRiskAssessmentBuilder::sameAssessments(
    const std::vector<ValidatorRiskAssessment>& left,
    const std::vector<ValidatorRiskAssessment>& right
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
