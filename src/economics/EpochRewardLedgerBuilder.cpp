#include "economics/EpochRewardLedgerBuilder.hpp"

#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::economics {

EpochRewardLedgerBuildResult::EpochRewardLedgerBuildResult()
    : m_distribution(),
      m_records() {}

EpochRewardLedgerBuildResult::EpochRewardLedgerBuildResult(
    EpochRewardDistribution distribution,
    std::vector<core::LedgerRecord> records
)
    : m_distribution(std::move(distribution)),
      m_records(std::move(records)) {}

const EpochRewardDistribution& EpochRewardLedgerBuildResult::distribution() const {
    return m_distribution;
}

const std::vector<core::LedgerRecord>& EpochRewardLedgerBuildResult::records() const {
    return m_records;
}

std::size_t EpochRewardLedgerBuildResult::recordCount() const {
    return m_records.size();
}

std::size_t EpochRewardLedgerBuildResult::genesisRewardRecordCount() const {
    std::size_t count = 0;

    for (const auto& record : m_records) {
        if (record.type() == core::LedgerRecordType::GENESIS_REWARD) {
            ++count;
        }
    }

    return count;
}

bool EpochRewardLedgerBuildResult::hasProtectionEpochRecord() const {
    for (const auto& record : m_records) {
        if (record.type() == core::LedgerRecordType::PROTECTION_EPOCH) {
            return true;
        }
    }

    return false;
}

bool EpochRewardLedgerBuildResult::isValid() const {
    if (!m_distribution.isValid()) {
        return false;
    }

    return EpochRewardLedgerBuilder::recordsMatchDistribution(
        m_distribution,
        m_records
    );
}

std::string EpochRewardLedgerBuildResult::serialize() const {
    std::ostringstream oss;

    oss << "EpochRewardLedgerBuildResult{"
        << "recordCount=" << recordCount()
        << ";hasProtectionEpochRecord=" << (hasProtectionEpochRecord() ? "true" : "false")
        << ";genesisRewardRecordCount=" << genesisRewardRecordCount()
        << ";distribution=" << m_distribution.serialize()
        << "}";

    return oss.str();
}

EpochRewardLedgerBuildResult EpochRewardLedgerBuilder::buildLedgerRecords(
    const EpochRewardDistribution& distribution,
    std::int64_t timestamp
) {
    if (!distribution.isValid()) {
        throw std::invalid_argument("Invalid EpochRewardDistribution rejected by ledger builder.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("Reward ledger record timestamp must be positive.");
    }

    std::vector<core::LedgerRecord> records;

    /*
     * Canonical ordering:
     * The epoch summary comes first. Reward births come after it.
     */
    records.push_back(
        core::LedgerRecord::fromProtectionEpoch(
            distribution.protectionEpoch(),
            timestamp
        )
    );

    for (const auto& rewardRecord : distribution.genesisRewardRecords()) {
        records.push_back(
            core::LedgerRecord::fromGenesisRewardRecord(
                rewardRecord,
                timestamp
            )
        );
    }

    EpochRewardLedgerBuildResult result(
        distribution,
        records
    );

    if (!result.isValid()) {
        throw std::logic_error("Epoch reward ledger builder produced invalid records.");
    }

    return result;
}

bool EpochRewardLedgerBuilder::recordsMatchDistribution(
    const EpochRewardDistribution& distribution,
    const std::vector<core::LedgerRecord>& records
) {
    if (!distribution.isValid()) {
        return false;
    }

    if (records.empty()) {
        return false;
    }

    for (const auto& record : records) {
        if (!record.isValid()) {
            return false;
        }
    }

    if (hasDuplicateRecordSourceIds(records)) {
        return false;
    }

    if (records.front().type() != core::LedgerRecordType::PROTECTION_EPOCH) {
        return false;
    }

    if (records.front().payload() != distribution.protectionEpoch().serialize()) {
        return false;
    }

    const std::vector<GenesisRewardRecord> rewardRecords =
        distribution.genesisRewardRecords();

    if (records.size() != rewardRecords.size() + 1U) {
        return false;
    }

    for (std::size_t index = 0; index < rewardRecords.size(); ++index) {
        const core::LedgerRecord& ledgerRecord =
            records[index + 1U];

        const GenesisRewardRecord& rewardRecord =
            rewardRecords[index];

        if (ledgerRecord.type() != core::LedgerRecordType::GENESIS_REWARD) {
            return false;
        }

        if (ledgerRecord.sourceId() != rewardRecord.deterministicId()) {
            return false;
        }

        if (ledgerRecord.payload() != rewardRecord.serialize()) {
            return false;
        }

        if (rewardRecord.epoch() != distribution.protectionEpoch().epochId()) {
            return false;
        }
    }

    return true;
}

bool EpochRewardLedgerBuilder::hasDuplicateRecordSourceIds(
    const std::vector<core::LedgerRecord>& records
) {
    std::set<std::string> sourceIds;

    for (const auto& record : records) {
        if (!sourceIds.insert(record.sourceId()).second) {
            return true;
        }
    }

    return false;
}

} // namespace nodo::economics
