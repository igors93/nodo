#include "node/SecurityCheckpoint.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

struct CheckpointAccumulator {
    std::uint16_t score = SECURITY_SCORE_MIN;
    utils::Amount lockedStake;
    std::uint64_t scoreRecordCount = 0;
    std::string sourceDigest;
};

std::string digestForCheckpoint(
    const std::string& validatorAddress,
    std::uint64_t blockHeight,
    std::uint16_t score,
    utils::Amount lockedStake,
    std::uint64_t scoreRecordCount
) {
    std::ostringstream oss;

    oss << "security-checkpoint:"
        << blockHeight
        << ":"
        << validatorAddress
        << ":"
        << score
        << ":"
        << lockedStake.rawUnits()
        << ":"
        << scoreRecordCount;

    return oss.str();
}

} // namespace

ValidatorSecurityCheckpoint::ValidatorSecurityCheckpoint()
    : m_validatorAddress(""),
      m_blockHeight(0),
      m_score(0),
      m_band(""),
      m_lockedStake(),
      m_securityScoreRecordCount(0),
      m_reason(""),
      m_sourceDigest("") {}

ValidatorSecurityCheckpoint::ValidatorSecurityCheckpoint(
    std::string validatorAddress,
    std::uint64_t blockHeight,
    std::uint16_t score,
    std::string band,
    utils::Amount lockedStake,
    std::uint64_t securityScoreRecordCount,
    std::string reason,
    std::string sourceDigest
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_blockHeight(blockHeight),
      m_score(score),
      m_band(std::move(band)),
      m_lockedStake(lockedStake),
      m_securityScoreRecordCount(securityScoreRecordCount),
      m_reason(std::move(reason)),
      m_sourceDigest(std::move(sourceDigest)) {}

const std::string& ValidatorSecurityCheckpoint::validatorAddress() const {
    return m_validatorAddress;
}

std::uint64_t ValidatorSecurityCheckpoint::blockHeight() const {
    return m_blockHeight;
}

std::uint16_t ValidatorSecurityCheckpoint::score() const {
    return m_score;
}

const std::string& ValidatorSecurityCheckpoint::band() const {
    return m_band;
}

utils::Amount ValidatorSecurityCheckpoint::lockedStake() const {
    return m_lockedStake;
}

std::uint64_t ValidatorSecurityCheckpoint::securityScoreRecordCount() const {
    return m_securityScoreRecordCount;
}

const std::string& ValidatorSecurityCheckpoint::reason() const {
    return m_reason;
}

const std::string& ValidatorSecurityCheckpoint::sourceDigest() const {
    return m_sourceDigest;
}

bool ValidatorSecurityCheckpoint::isValid() const {
    if (m_validatorAddress.empty() ||
        m_blockHeight == 0 ||
        m_score < SECURITY_SCORE_MIN ||
        m_score > SECURITY_SCORE_MAX ||
        m_band != ValidatorSecurityCheckpointBuilder::bandForScore(m_score) ||
        m_lockedStake.isNegative() ||
        m_securityScoreRecordCount == 0 ||
        m_reason != ValidatorSecurityCheckpointBuilder::SECURITY_CHECKPOINT_REASON ||
        m_sourceDigest.empty()) {
        return false;
    }

    return m_sourceDigest == digestForCheckpoint(
        m_validatorAddress,
        m_blockHeight,
        m_score,
        m_lockedStake,
        m_securityScoreRecordCount
    );
}

std::string ValidatorSecurityCheckpoint::serialize() const {
    std::ostringstream oss;

    oss << "ValidatorSecurityCheckpoint{"
        << "validatorAddress=" << m_validatorAddress
        << ";blockHeight=" << m_blockHeight
        << ";score=" << m_score
        << ";band=" << m_band
        << ";lockedStakeRawUnits=" << m_lockedStake.rawUnits()
        << ";securityScoreRecordCount=" << m_securityScoreRecordCount
        << ";reason=" << m_reason
        << ";sourceDigest=" << m_sourceDigest
        << "}";

    return oss.str();
}

std::string ValidatorSecurityCheckpointBuilder::bandForScore(
    std::uint16_t score
) {
    if (score >= 900) {
        return "ELITE";
    }

    if (score >= 700) {
        return "STRONG";
    }

    if (score >= 400) {
        return "STABLE";
    }

    if (score >= 100) {
        return "WATCHED";
    }

    return "BUILDING";
}

std::vector<ValidatorSecurityCheckpoint> ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(
    const std::vector<SecurityScoreRecord>& securityScoreRecords,
    const std::vector<LockedStakePosition>& lockedStakePositions,
    std::uint64_t blockHeight
) {
    if (blockHeight == 0) {
        throw std::invalid_argument("Cannot build security checkpoints at genesis height.");
    }

    if (securityScoreRecords.empty()) {
        return {};
    }

    std::map<std::string, CheckpointAccumulator> accumulators;

    for (const SecurityScoreRecord& record : securityScoreRecords) {
        if (!record.isValid()) {
            throw std::invalid_argument("Cannot build security checkpoint from invalid security score record.");
        }

        if (record.blockHeight() != blockHeight) {
            throw std::invalid_argument("Security score record height does not match checkpoint height.");
        }

        CheckpointAccumulator& accumulator =
            accumulators[record.validatorAddress()];

        accumulator.score =
            std::max(
                accumulator.score,
                record.score()
            );

        ++accumulator.scoreRecordCount;
    }

    for (const LockedStakePosition& position : lockedStakePositions) {
        if (!position.isValid()) {
            throw std::invalid_argument("Cannot build security checkpoint from invalid locked stake position.");
        }

        auto iterator =
            accumulators.find(position.ownerAddress());

        if (iterator == accumulators.end()) {
            continue;
        }

        iterator->second.lockedStake =
            iterator->second.lockedStake + position.amount();
    }

    std::vector<ValidatorSecurityCheckpoint> checkpoints;
    checkpoints.reserve(accumulators.size());

    for (auto& entry : accumulators) {
        const std::string& validatorAddress =
            entry.first;

        CheckpointAccumulator& accumulator =
            entry.second;

        accumulator.sourceDigest =
            digestForCheckpoint(
                validatorAddress,
                blockHeight,
                accumulator.score,
                accumulator.lockedStake,
                accumulator.scoreRecordCount
            );

        checkpoints.emplace_back(
            validatorAddress,
            blockHeight,
            accumulator.score,
            bandForScore(accumulator.score),
            accumulator.lockedStake,
            accumulator.scoreRecordCount,
            SECURITY_CHECKPOINT_REASON,
            accumulator.sourceDigest
        );
    }

    return checkpoints;
}

bool ValidatorSecurityCheckpointBuilder::sameCheckpoints(
    const std::vector<ValidatorSecurityCheckpoint>& left,
    const std::vector<ValidatorSecurityCheckpoint>& right
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
