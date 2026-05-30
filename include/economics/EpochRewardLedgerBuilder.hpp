#ifndef NODO_ECONOMICS_EPOCH_REWARD_LEDGER_BUILDER_HPP
#define NODO_ECONOMICS_EPOCH_REWARD_LEDGER_BUILDER_HPP

#include "core/LedgerRecord.hpp"
#include "economics/EpochRewardDistributor.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::economics {

/*
 * EpochRewardLedgerBuildResult is the deterministic bridge between a reward
 * distribution and ledger records.
 *
 * In simple terms:
 * EpochRewardDistributor calculates rewards.
 * EpochRewardLedgerBuilder turns that calculation into official LedgerRecords.
 */
class EpochRewardLedgerBuildResult {
public:
    EpochRewardLedgerBuildResult();

    EpochRewardLedgerBuildResult(
        EpochRewardDistribution distribution,
        std::vector<core::LedgerRecord> records
    );

    const EpochRewardDistribution& distribution() const;
    const std::vector<core::LedgerRecord>& records() const;

    std::size_t recordCount() const;
    std::size_t genesisRewardRecordCount() const;

    bool hasProtectionEpochRecord() const;
    bool isValid() const;

    std::string serialize() const;

private:
    EpochRewardDistribution m_distribution;
    std::vector<core::LedgerRecord> m_records;
};

/*
 * EpochRewardLedgerBuilder converts a valid EpochRewardDistribution into
 * LedgerRecords in a canonical order:
 *
 * 1. PROTECTION_EPOCH
 * 2. GENESIS_REWARD records sorted by distributor output order
 *
 * Security principle:
 * Reward records must be deterministic and auditable before they can enter a
 * block proposal.
 */
class EpochRewardLedgerBuilder {
public:
    static EpochRewardLedgerBuildResult buildLedgerRecords(
        const EpochRewardDistribution& distribution,
        std::int64_t timestamp
    );

    static bool recordsMatchDistribution(
        const EpochRewardDistribution& distribution,
        const std::vector<core::LedgerRecord>& records
    );

private:
    static bool hasDuplicateRecordSourceIds(
        const std::vector<core::LedgerRecord>& records
    );
};

} // namespace nodo::economics

#endif
