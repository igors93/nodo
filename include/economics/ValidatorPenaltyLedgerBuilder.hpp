#ifndef NODO_ECONOMICS_VALIDATOR_PENALTY_LEDGER_BUILDER_HPP
#define NODO_ECONOMICS_VALIDATOR_PENALTY_LEDGER_BUILDER_HPP

#include "core/LedgerRecord.hpp"
#include "economics/ValidatorPenaltyRecord.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::core {
class ValidatorDoubleSignEvidence;
}

namespace nodo::economics {

/*
 * ValidatorPenaltyLedgerBuildResult is the bridge between evidence and ledger
 * records.
 *
 * Canonical order:
 * 1. VALIDATOR_PENALTY
 * 2. VALIDATOR_SCORE
 */
class ValidatorPenaltyLedgerBuildResult {
public:
    ValidatorPenaltyLedgerBuildResult();

    ValidatorPenaltyLedgerBuildResult(
        ValidatorPenaltyRecord penaltyRecord,
        ValidatorScoreRecord scoreRecord,
        std::vector<core::LedgerRecord> records
    );

    const ValidatorPenaltyRecord& penaltyRecord() const;
    const ValidatorScoreRecord& scoreRecord() const;
    const std::vector<core::LedgerRecord>& records() const;

    bool isValid() const;

    std::string serialize() const;

private:
    ValidatorPenaltyRecord m_penaltyRecord;
    ValidatorScoreRecord m_scoreRecord;
    std::vector<core::LedgerRecord> m_records;
};

class ValidatorPenaltyLedgerBuilder {
public:
    static ValidatorPenaltyLedgerBuildResult buildDoubleSignPenaltyRecords(
        const core::ValidatorDoubleSignEvidence& evidence,
        const ValidatorPenaltyPolicy& policy,
        std::uint64_t epoch,
        std::int32_t previousScore,
        std::int64_t timestamp
    );

    static bool recordsMatchPenalty(
        const ValidatorPenaltyRecord& penaltyRecord,
        const ValidatorScoreRecord& scoreRecord,
        const std::vector<core::LedgerRecord>& records
    );
};

} // namespace nodo::economics

#endif
