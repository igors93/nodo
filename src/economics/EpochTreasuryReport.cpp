#include "economics/EpochTreasuryReport.hpp"

#include "crypto/hash.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>

namespace nodo::economics {

namespace {

// Compute a deterministic SHA-256 digest over the canonical spend record set.
// Records are sorted by spendId before hashing so that insertion order does
// not affect the digest.
std::string computeSpendRecordsDigest(
    const std::vector<TreasurySpendRecord>& records
) {
    if (records.empty()) {
        // Canonical empty digest: hash of the empty string.
        char buf[NODO_HASH_BUFFER_SIZE] = {};
        nodo_hash_string("treasury-spend-records:empty", buf, NODO_HASH_BUFFER_SIZE);
        return std::string(buf);
    }

    // Sort records by spendId for canonical ordering.
    std::vector<const TreasurySpendRecord*> sorted;
    sorted.reserve(records.size());
    for (const auto& rec : records) {
        sorted.push_back(&rec);
    }
    std::sort(sorted.begin(), sorted.end(),
        [](const TreasurySpendRecord* a, const TreasurySpendRecord* b) {
            return a->spendId() < b->spendId();
        }
    );

    std::ostringstream canonical;
    canonical << "treasury-spend-records";
    for (const auto* rec : sorted) {
        canonical << "|"
                  << rec->spendId() << ":"
                  << rec->proposalId() << ":"
                  << rec->recipientAddress() << ":"
                  << rec->amount().rawUnits() << ":"
                  << rec->executedAtBlock() << ":"
                  << rec->epoch();
    }

    char buf[NODO_HASH_BUFFER_SIZE] = {};
    nodo_hash_string(canonical.str().c_str(), buf, NODO_HASH_BUFFER_SIZE);
    return std::string(buf);
}

} // namespace

EpochTreasuryReport::EpochTreasuryReport()
    : m_epoch(0),
      m_treasurySpendTotal(utils::Amount::fromRawUnits(0)),
      m_spendRecordCount(0),
      m_hasSpendRecords(false),
      m_valid(false),
      m_rejectionReason("EpochTreasuryReport: default-constructed.") {}

EpochTreasuryReport EpochTreasuryReport::fromSpendRecords(
    std::uint64_t epoch,
    const std::vector<TreasurySpendRecord>& spendRecords
) {
    EpochTreasuryReport r;
    r.m_epoch = epoch;
    r.m_spendRecordCount = spendRecords.size();
    r.m_rejectionReason = "";

    std::int64_t runningTotal = 0;
    for (std::size_t i = 0; i < spendRecords.size(); ++i) {
        const auto& rec = spendRecords[i];
        if (!rec.isValid()) {
            r.m_rejectionReason =
                "EpochTreasuryReport: spend record at index " +
                std::to_string(i) + " is invalid: " + rec.rejectionReason();
            return r;
        }
        if (rec.amount().rawUnits() >
            std::numeric_limits<std::int64_t>::max() - runningTotal) {
            r.m_rejectionReason =
                "EpochTreasuryReport: treasury spend total would overflow int64 "
                "at record index " + std::to_string(i);
            return r;
        }
        runningTotal += rec.amount().rawUnits();
    }

    r.m_treasurySpendTotal = utils::Amount::fromRawUnits(runningTotal);
    r.m_spendRecords = spendRecords;
    r.m_spendRecordsDigest = computeSpendRecordsDigest(spendRecords);
    r.m_hasSpendRecords = true;
    r.m_valid = true;
    return r;
}

EpochTreasuryReport EpochTreasuryReport::fromStoredFields(
    std::uint64_t epoch,
    utils::Amount treasurySpendTotal,
    std::size_t spendRecordCount,
    std::string spendRecordsDigest
) {
    EpochTreasuryReport r;
    r.m_epoch = epoch;
    r.m_treasurySpendTotal = treasurySpendTotal;
    r.m_spendRecordCount = spendRecordCount;
    r.m_spendRecordsDigest = std::move(spendRecordsDigest);
    r.m_hasSpendRecords = false;
    r.m_rejectionReason = "";
    r.m_valid = true;
    return r;
}

std::uint64_t EpochTreasuryReport::epoch() const { return m_epoch; }
utils::Amount EpochTreasuryReport::treasurySpendTotal() const { return m_treasurySpendTotal; }
std::size_t EpochTreasuryReport::spendRecordCount() const { return m_spendRecordCount; }
const std::vector<TreasurySpendRecord>& EpochTreasuryReport::spendRecords() const { return m_spendRecords; }
const std::string& EpochTreasuryReport::spendRecordsDigest() const { return m_spendRecordsDigest; }
bool EpochTreasuryReport::hasSpendRecords() const { return m_hasSpendRecords; }
bool EpochTreasuryReport::isValid() const { return m_valid; }
const std::string& EpochTreasuryReport::rejectionReason() const { return m_rejectionReason; }

std::string EpochTreasuryReport::serialize() const {
    std::ostringstream oss;
    oss << "EpochTreasuryReport{"
        << "epoch=" << m_epoch
        << ";treasurySpendTotalRaw=" << m_treasurySpendTotal.rawUnits()
        << ";spendRecordCount=" << m_spendRecordCount
        << ";valid=" << (m_valid ? "1" : "0")
        << ";spendRecordsDigest=" << m_spendRecordsDigest
        << "}";
    return oss.str();
}

} // namespace nodo::economics
