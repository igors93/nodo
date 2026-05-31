#ifndef NODO_NODE_SECURITY_SCORE_HPP
#define NODO_NODE_SECURITY_SCORE_HPP

#include "node/LockedStakePosition.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::node {

constexpr std::uint16_t SECURITY_SCORE_MIN = 1;
constexpr std::uint16_t SECURITY_SCORE_MAX = 1000;
constexpr std::uint16_t SECURITY_SCORE_BLOCK_PARTICIPATION_POINTS = 2;

class SecurityScoreRecord {
public:
    SecurityScoreRecord();

    SecurityScoreRecord(
        std::string validatorAddress,
        std::uint64_t blockHeight,
        std::uint16_t score,
        std::uint16_t lockedStakeScore,
        std::uint16_t participationScore,
        std::uint16_t maturityScore,
        std::uint16_t penaltyScore,
        std::string reason,
        std::string sourceLockedStakeId
    );

    const std::string& validatorAddress() const;
    std::uint64_t blockHeight() const;
    std::uint16_t score() const;
    std::uint16_t lockedStakeScore() const;
    std::uint16_t participationScore() const;
    std::uint16_t maturityScore() const;
    std::uint16_t penaltyScore() const;
    const std::string& reason() const;
    const std::string& sourceLockedStakeId() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::uint64_t m_blockHeight;
    std::uint16_t m_score;
    std::uint16_t m_lockedStakeScore;
    std::uint16_t m_participationScore;
    std::uint16_t m_maturityScore;
    std::uint16_t m_penaltyScore;
    std::string m_reason;
    std::string m_sourceLockedStakeId;
};

class SecurityScoreCalculator {
public:
    static constexpr const char* LOCKED_STAKE_REWARD_REASON =
        "LOCKED_STAKE_REWARD";

    static std::uint16_t lockedStakePoints(
        const LockedStakePosition& position
    );

    static SecurityScoreRecord buildFromLockedStakePosition(
        const LockedStakePosition& position,
        std::uint64_t blockHeight
    );

    static std::vector<SecurityScoreRecord> buildFromLockedStakePositions(
        const std::vector<LockedStakePosition>& positions,
        std::uint64_t blockHeight
    );

    static bool sameRecords(
        const std::vector<SecurityScoreRecord>& left,
        const std::vector<SecurityScoreRecord>& right
    );
};

} // namespace nodo::node

#endif
