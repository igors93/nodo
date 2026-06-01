#ifndef NODO_ECONOMICS_SUPPLY_DELTA_BUILDER_HPP
#define NODO_ECONOMICS_SUPPLY_DELTA_BUILDER_HPP

#include "economics/BurnRecord.hpp"
#include "economics/MintRecord.hpp"
#include "economics/SupplyDelta.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::economics {

/*
 * SupplyDeltaBuilder computes mintedAmount, burnedAmount, and supplyAfter
 * from the provided records, preventing arithmetic mistakes at call sites.
 *
 * Security principle:
 * Manual construction of SupplyDelta requires callers to compute three
 * derived fields (mintedAmount, burnedAmount, supplyAfter) by hand. Any
 * arithmetic mistake passes silently until isValid() catches the mismatch.
 * This builder eliminates that class of error for production paths.
 *
 * Important:
 * The low-level SupplyDelta constructor remains available so that tests can
 * build intentionally invalid deltas for negative testing. Do not remove it.
 */
class SupplyDeltaBuilder {
public:
    /*
     * Build a SupplyDelta by computing all derived amounts from the records.
     *
     * The returned delta may still be invalid if record consistency checks
     * fail (wrong epoch/blockHeight/blockHash). Call isValid() to verify.
     *
     * Throws std::overflow_error if the record sum would overflow int64.
     */
    static SupplyDelta build(
        std::uint64_t blockHeight,
        const std::string& blockHash,
        std::uint64_t epoch,
        utils::Amount supplyBefore,
        const std::vector<MintRecord>& mintRecords,
        const std::vector<BurnRecord>& burnRecords
    );
};

} // namespace nodo::economics

#endif
