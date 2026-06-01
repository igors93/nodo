#ifndef NODO_ECONOMICS_SUPPLY_DELTA_HPP
#define NODO_ECONOMICS_SUPPLY_DELTA_HPP

#include "economics/BurnRecord.hpp"
#include "economics/MintRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::economics {

/*
 * SupplyDelta describes the complete supply transition for one block.
 *
 * It proves:
 *   supplyAfter == supplyBefore + mintedAmount - burnedAmount
 *
 * with supporting evidence in the form of individual MintRecords and
 * BurnRecords. Both amounts must match the sum of their records.
 *
 * Security principle:
 * A delta without matching records, or whose arithmetic does not close, must
 * be rejected. A node that accepts a malformed delta allows untracked supply
 * creation or destruction, which is a critical monetary invariant violation.
 *
 * Overflow/underflow handling:
 * All arithmetic uses int64_t. A sum that would exceed INT64_MAX is treated
 * as a supply overflow. A supplyAfter that goes negative is an underflow.
 * Both are rejected.
 */
class SupplyDelta {
public:
    SupplyDelta();

    SupplyDelta(
        std::uint64_t blockHeight,
        std::string blockHash,
        std::uint64_t epoch,
        utils::Amount supplyBefore,
        utils::Amount mintedAmount,
        utils::Amount burnedAmount,
        utils::Amount supplyAfter,
        std::vector<MintRecord> mintRecords,
        std::vector<BurnRecord> burnRecords
    );

    static SupplyDelta noOp(
        std::uint64_t blockHeight,
        const std::string& blockHash,
        std::uint64_t epoch,
        utils::Amount currentSupply
    );

    std::uint64_t blockHeight() const;
    const std::string& blockHash() const;
    std::uint64_t epoch() const;
    utils::Amount supplyBefore() const;
    utils::Amount mintedAmount() const;
    utils::Amount burnedAmount() const;
    utils::Amount supplyAfter() const;
    const std::vector<MintRecord>& mintRecords() const;
    const std::vector<BurnRecord>& burnRecords() const;

    bool isValid() const;
    std::string rejectionReason() const;
    std::string serialize() const;

private:
    std::uint64_t m_blockHeight;
    std::string m_blockHash;
    std::uint64_t m_epoch;
    utils::Amount m_supplyBefore;
    utils::Amount m_mintedAmount;
    utils::Amount m_burnedAmount;
    utils::Amount m_supplyAfter;
    std::vector<MintRecord> m_mintRecords;
    std::vector<BurnRecord> m_burnRecords;
};

} // namespace nodo::economics

#endif
