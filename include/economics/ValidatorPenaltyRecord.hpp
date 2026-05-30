#ifndef NODO_ECONOMICS_VALIDATOR_PENALTY_RECORD_HPP
#define NODO_ECONOMICS_VALIDATOR_PENALTY_RECORD_HPP

#include "economics/ValidatorScoreRecord.hpp"

#include <cstdint>
#include <string>

namespace nodo::core {
class ValidatorDoubleSignEvidence;
}

namespace nodo::economics {

/*
 * ValidatorPenaltyReason describes why a validator penalty was produced.
 *
 * Security principle:
 * Penalties must be based on auditable evidence, not on silent local decisions.
 */
enum class ValidatorPenaltyReason {
    UNKNOWN,
    DOUBLE_SIGN,
    INVALID_PROPOSAL,
    INVALID_SIGNATURE,
    MANUAL_REVIEW
};

std::string validatorPenaltyReasonToString(
    ValidatorPenaltyReason reason
);

ValidatorPenaltyReason validatorPenaltyReasonFromString(
    const std::string& value
);

/*
 * ValidatorPenaltyAction describes the first consequence recommended by the
 * penalty record.
 *
 * This phase applies score penalties. Future phases can connect the same record
 * to locked stake and CoinLot slashing once stake locks are implemented.
 */
enum class ValidatorPenaltyAction {
    UNKNOWN,
    SCORE_REDUCTION,
    SLASHING_REVIEW,
    SECURITY_LOCK_REVIEW
};

std::string validatorPenaltyActionToString(
    ValidatorPenaltyAction action
);

ValidatorPenaltyAction validatorPenaltyActionFromString(
    const std::string& value
);

/*
 * ValidatorPenaltyRecord is the auditable record created from validator
 * misbehavior evidence.
 *
 * For double-signing, the record commits to both conflicting block hashes and
 * both signature digests. That makes the penalty reproducible from evidence.
 */
class ValidatorPenaltyRecord {
public:
    ValidatorPenaltyRecord();

    ValidatorPenaltyRecord(
        std::string validatorAddress,
        std::uint64_t epoch,
        std::uint64_t blockIndex,
        std::int32_t previousScore,
        std::int32_t newScore,
        ValidatorPenaltyReason reason,
        ValidatorPenaltyAction action,
        std::string evidenceHash,
        std::string firstBlockHash,
        std::string conflictingBlockHash,
        std::string firstSignatureDigest,
        std::string conflictingSignatureDigest,
        std::int64_t timestamp
    );

    const std::string& validatorAddress() const;
    std::uint64_t epoch() const;
    std::uint64_t blockIndex() const;
    std::int32_t previousScore() const;
    std::int32_t newScore() const;
    ValidatorPenaltyReason reason() const;
    ValidatorPenaltyAction action() const;
    const std::string& evidenceHash() const;
    const std::string& firstBlockHash() const;
    const std::string& conflictingBlockHash() const;
    const std::string& firstSignatureDigest() const;
    const std::string& conflictingSignatureDigest() const;
    std::int64_t timestamp() const;

    std::int32_t scorePenalty() const;

    bool isValid() const;

    std::string deterministicId() const;

    ValidatorScoreRecord createScoreRecord() const;

    std::string serialize() const;

    static ValidatorPenaltyRecord deserialize(
        const std::string& serialized
    );

private:
    static bool isScoreInRange(
        std::int32_t score
    );

    std::string m_validatorAddress;
    std::uint64_t m_epoch;
    std::uint64_t m_blockIndex;
    std::int32_t m_previousScore;
    std::int32_t m_newScore;
    ValidatorPenaltyReason m_reason;
    ValidatorPenaltyAction m_action;
    std::string m_evidenceHash;
    std::string m_firstBlockHash;
    std::string m_conflictingBlockHash;
    std::string m_firstSignatureDigest;
    std::string m_conflictingSignatureDigest;
    std::int64_t m_timestamp;
};

/*
 * ValidatorPenaltyPolicy converts detected evidence into deterministic penalty
 * records.
 *
 * The policy is intentionally small and explicit so future governance can audit
 * exactly how a penalty was calculated.
 */
class ValidatorPenaltyPolicy {
public:
    static ValidatorPenaltyPolicy conservativeDefaultPolicy();

    explicit ValidatorPenaltyPolicy(
        std::int32_t doubleSignScorePenalty
    );

    std::int32_t doubleSignScorePenalty() const;

    bool isValid() const;

    std::int32_t applyDoubleSignPenalty(
        std::int32_t previousScore
    ) const;

    ValidatorPenaltyRecord createDoubleSignPenaltyRecord(
        const core::ValidatorDoubleSignEvidence& evidence,
        std::uint64_t epoch,
        std::int32_t previousScore,
        std::int64_t timestamp
    ) const;

    std::string serialize() const;

private:
    std::int32_t m_doubleSignScorePenalty;
};

} // namespace nodo::economics

#endif
