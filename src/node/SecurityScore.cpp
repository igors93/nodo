#include "node/SecurityScore.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::uint16_t clampScore(
    std::uint64_t score
) {
    if (score < SECURITY_SCORE_MIN) {
        return SECURITY_SCORE_MIN;
    }

    if (score > SECURITY_SCORE_MAX) {
        return SECURITY_SCORE_MAX;
    }

    return static_cast<std::uint16_t>(score);
}

} // namespace

SecurityScoreRecord::SecurityScoreRecord()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_score(0),
      m_lockedStakeScore(0),
      m_participationScore(0),
      m_maturityScore(0),
      m_penaltyScore(0),
      m_reason(""),
      m_sourceLockedStakeId("") {}

SecurityScoreRecord::SecurityScoreRecord(
    std::string validatorAddress,
    std::uint64_t blockHeight,
    std::uint16_t score,
    std::uint16_t lockedStakeScore,
    std::uint16_t participationScore,
    std::uint16_t maturityScore,
    std::uint16_t penaltyScore,
    std::string reason,
    std::string sourceLockedStakeId
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_score(score),
      m_lockedStakeScore(lockedStakeScore),
      m_participationScore(participationScore),
      m_maturityScore(maturityScore),
      m_penaltyScore(penaltyScore),
      m_reason(std::move(reason)),
      m_sourceLockedStakeId(std::move(sourceLockedStakeId)) {}

const std::string& SecurityScoreRecord::validatorAddress() const {
    return m_validatorAddress;
}

std::uint64_t SecurityScoreRecord::blockHeight() const {
    return m_blockHeight;
}

std::uint16_t SecurityScoreRecord::score() const {
    return m_score;
}

std::uint16_t SecurityScoreRecord::lockedStakeScore() const {
    return m_lockedStakeScore;
}

std::uint16_t SecurityScoreRecord::participationScore() const {
    return m_participationScore;
}

std::uint16_t SecurityScoreRecord::maturityScore() const {
    return m_maturityScore;
}

std::uint16_t SecurityScoreRecord::penaltyScore() const {
    return m_penaltyScore;
}

const std::string& SecurityScoreRecord::reason() const {
    return m_reason;
}

const std::string& SecurityScoreRecord::sourceLockedStakeId() const {
    return m_sourceLockedStakeId;
}

bool SecurityScoreRecord::isValid() const {
    if (m_validatorAddress.empty() ||
        m_blockHeight == 0 ||
        m_score < SECURITY_SCORE_MIN ||
        m_score > SECURITY_SCORE_MAX ||
        m_reason != SecurityScoreCalculator::LOCKED_STAKE_REWARD_REASON ||
        m_sourceLockedStakeId.empty()) {
        return false;
    }

    const std::uint64_t rawScore =
        static_cast<std::uint64_t>(SECURITY_SCORE_MIN)
        + m_lockedStakeScore
        + m_participationScore
        + m_maturityScore;

    const std::uint64_t adjustedScore =
        rawScore > m_penaltyScore
            ? rawScore - m_penaltyScore
            : SECURITY_SCORE_MIN;

    return m_score == clampScore(adjustedScore);
}

std::string SecurityScoreRecord::serialize() const {
    std::ostringstream oss;

    oss << "SecurityScoreRecord{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";score=" << m_score
        << ";lockedStakeScore=" << m_lockedStakeScore
        << ";participationScore=" << m_participationScore
        << ";maturityScore=" << m_maturityScore
        << ";penaltyScore=" << m_penaltyScore
        << ";reason=" << m_reason
        << ";sourceLockedStakeId=" << m_sourceLockedStakeId
        << "}";

    return oss.str();
}

std::uint16_t SecurityScoreCalculator::lockedStakePoints(
    const LockedStakePosition& position
) {
    if (!position.isValid()) {
        throw std::invalid_argument("Cannot score invalid locked stake position.");
    }

    const std::int64_t rawAmount =
        position.amount().rawUnits();

    if (rawAmount <= 0) {
        throw std::invalid_argument("Cannot score non-positive locked stake amount.");
    }

    /*
     * Security score must be earned slowly. The first version gives one point
     * per ten locked raw units and caps this component at 300 points.
     */
    const std::int64_t points =
        rawAmount / 10;

    return static_cast<std::uint16_t>(
        std::min<std::int64_t>(
            300,
            std::max<std::int64_t>(
                1,
                points
            )
        )
    );
}

SecurityScoreRecord SecurityScoreCalculator::buildFromLockedStakePosition(
    const LockedStakePosition& position,
    std::uint64_t blockHeight
) {
    if (!position.isValid()) {
        throw std::invalid_argument("Cannot build security score from invalid locked stake position.");
    }

    if (blockHeight == 0) {
        throw std::invalid_argument("Cannot build security score at genesis height.");
    }

    const std::uint16_t lockedStakeScore =
        lockedStakePoints(position);

    const std::uint16_t participationScore =
        SECURITY_SCORE_BLOCK_PARTICIPATION_POINTS;

    const std::uint16_t maturityScore =
        position.isMatureAt(blockHeight) ? 10 : 0;

    const std::uint16_t penaltyScore =
        0;

    const std::uint64_t rawScore =
        static_cast<std::uint64_t>(SECURITY_SCORE_MIN)
        + lockedStakeScore
        + participationScore
        + maturityScore;

    return SecurityScoreRecord(
        position.ownerAddress(),
        blockHeight,
        clampScore(rawScore),
        lockedStakeScore,
        participationScore,
        maturityScore,
        penaltyScore,
        LOCKED_STAKE_REWARD_REASON,
        position.sourceRewardId()
    );
}

std::vector<SecurityScoreRecord> SecurityScoreCalculator::buildFromLockedStakePositions(
    const std::vector<LockedStakePosition>& positions,
    std::uint64_t blockHeight
) {
    std::vector<SecurityScoreRecord> records;
    records.reserve(positions.size());

    for (const LockedStakePosition& position : positions) {
        records.push_back(
            buildFromLockedStakePosition(
                position,
                blockHeight
            )
        );
    }

    return records;
}

bool SecurityScoreCalculator::sameRecords(
    const std::vector<SecurityScoreRecord>& left,
    const std::vector<SecurityScoreRecord>& right
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
