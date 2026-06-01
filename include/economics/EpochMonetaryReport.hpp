#ifndef NODO_ECONOMICS_EPOCH_MONETARY_REPORT_HPP
#define NODO_ECONOMICS_EPOCH_MONETARY_REPORT_HPP

#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDelta.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::economics {

/*
 * EpochMonetaryReport summarizes finalized SupplyDelta records for one epoch.
 *
 * Foundation for Task 06 and beyond. Does not implement full treasury/reward
 * accounting yet. The report is deterministic and derivable from the SupplyDelta
 * sequence.
 */
class EpochMonetaryReport {
public:
    EpochMonetaryReport();

    static EpochMonetaryReport fromDeltas(
        const MonetaryPolicy& policy,
        std::uint64_t epoch,
        std::uint64_t startBlock,
        std::uint64_t endBlock,
        const std::vector<SupplyDelta>& deltas
    );

    std::uint64_t epoch() const;
    std::uint64_t startBlock() const;
    std::uint64_t endBlock() const;
    utils::Amount startingSupply() const;
    utils::Amount endingSupply() const;
    utils::Amount totalMinted() const;
    utils::Amount totalBurned() const;
    std::size_t deltaCount() const;
    const std::string& policyVersion() const;

    bool isValid() const;
    std::string rejectionReason() const;
    std::string serialize() const;

private:
    std::uint64_t m_epoch;
    std::uint64_t m_startBlock;
    std::uint64_t m_endBlock;
    utils::Amount m_startingSupply;
    utils::Amount m_endingSupply;
    utils::Amount m_totalMinted;
    utils::Amount m_totalBurned;
    std::size_t m_deltaCount;
    std::string m_policyVersion;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
