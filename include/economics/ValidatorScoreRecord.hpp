#ifndef NODO_ECONOMICS_VALIDATOR_SCORE_RECORD_HPP
#define NODO_ECONOMICS_VALIDATOR_SCORE_RECORD_HPP

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * ValidatorScoreRecord records an on-chain validator trust update.
 *
 * Score is trust, not automatic income.
 *
 * The score is intentionally bounded from 0 to 100.
 */
enum class ValidatorScoreReason {
    UNKNOWN,
    INITIAL_REGISTRATION,
    CONSISTENT_VALIDATION,
    SUCCESSFUL_CHALLENGE_RESPONSE,
    USEFUL_STORAGE_SERVICE,
    NETWORK_CLUSTER_PENALTY,
    MISSED_CHALLENGE,
    INVALID_WORK,
    CONFLICTING_SIGNATURE,
    MANUAL_REVIEW
};

std::string validatorScoreReasonToString(
    ValidatorScoreReason reason
);

ValidatorScoreReason validatorScoreReasonFromString(
    const std::string& value
);

class ValidatorScoreRecord {
public:
    static constexpr std::int32_t MIN_SCORE = 0;
    static constexpr std::int32_t MAX_SCORE = 100;

    ValidatorScoreRecord();

    ValidatorScoreRecord(
        std::string validatorAddress,
        std::uint64_t epoch,
        std::int32_t previousScore,
        std::int32_t newScore,
        ValidatorScoreReason reason,
        std::string evidenceHash,
        std::int64_t timestamp
    );

    const std::string& validatorAddress() const;
    std::uint64_t epoch() const;
    std::int32_t previousScore() const;
    std::int32_t newScore() const;
    ValidatorScoreReason reason() const;
    const std::string& evidenceHash() const;
    std::int64_t timestamp() const;

    std::int32_t scoreDelta() const;

    /*
     * 0 score -> 0 bps
     * 100 score -> 10000 bps
     */
    std::uint32_t trustFactorBasisPoints() const;

    bool increased() const;
    bool decreased() const;

    bool isValid() const;

    std::string serialize() const;

    static ValidatorScoreRecord deserialize(
        const std::string& serialized
    );

private:
    static bool isScoreInRange(
        std::int32_t score
    );

    std::string m_validatorAddress;
    std::uint64_t m_epoch;
    std::int32_t m_previousScore;
    std::int32_t m_newScore;
    ValidatorScoreReason m_reason;
    std::string m_evidenceHash;
    std::int64_t m_timestamp;
};

} // namespace nodo::economics

#endif
