#ifndef NODO_NODE_SECURITY_CHECKPOINT_HPP
#define NODO_NODE_SECURITY_CHECKPOINT_HPP

#include "node/LockedStakePosition.hpp"
#include "node/SecurityScore.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

class ValidatorSecurityCheckpoint {
public:
    ValidatorSecurityCheckpoint();

    ValidatorSecurityCheckpoint(
        std::string validatorAddress,
        std::uint64_t blockHeight,
        std::uint16_t score,
        std::string band,
        utils::Amount lockedStake,
        std::uint64_t securityScoreRecordCount,
        std::string reason,
        std::string sourceDigest
    );

    const std::string& validatorAddress() const;
    std::uint64_t blockHeight() const;
    std::uint16_t score() const;
    const std::string& band() const;
    utils::Amount lockedStake() const;
    std::uint64_t securityScoreRecordCount() const;
    const std::string& reason() const;
    const std::string& sourceDigest() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_blockHeight;
    std::uint16_t m_score;
    std::string m_band;
    utils::Amount m_lockedStake;
    std::uint64_t m_securityScoreRecordCount;
    std::string m_reason;
    std::string m_sourceDigest;
};

class ValidatorSecurityCheckpointBuilder {
public:
    static constexpr const char* SECURITY_CHECKPOINT_REASON =
        "SECURITY_SCORE_CHECKPOINT";

    static std::string bandForScore(
        std::uint16_t score
    );

    static std::vector<ValidatorSecurityCheckpoint> buildFromSecurityScores(
        const std::vector<SecurityScoreRecord>& securityScoreRecords,
        const std::vector<LockedStakePosition>& lockedStakePositions,
        std::uint64_t blockHeight
    );

    static bool sameCheckpoints(
        const std::vector<ValidatorSecurityCheckpoint>& left,
        const std::vector<ValidatorSecurityCheckpoint>& right
    );
};

} // namespace nodo::node

#endif
