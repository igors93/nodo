#include "economics/SupplyDeltaBuilder.hpp"

#include <climits>
#include <stdexcept>

namespace nodo::economics {

SupplyDelta SupplyDeltaBuilder::build(
    std::uint64_t blockHeight,
    const std::string& blockHash,
    std::uint64_t epoch,
    utils::Amount supplyBefore,
    const std::vector<MintRecord>& mintRecords,
    const std::vector<BurnRecord>& burnRecords
) {
    std::int64_t mintSum = 0;
    for (const auto& mint : mintRecords) {
        if (mint.amount().rawUnits() > 0 &&
            mintSum > INT64_MAX - mint.amount().rawUnits()) {
            throw std::overflow_error(
                "SupplyDeltaBuilder: mint records sum overflows int64."
            );
        }
        mintSum += mint.amount().rawUnits();
    }

    std::int64_t burnSum = 0;
    for (const auto& burn : burnRecords) {
        if (burn.amount().rawUnits() > 0 &&
            burnSum > INT64_MAX - burn.amount().rawUnits()) {
            throw std::overflow_error(
                "SupplyDeltaBuilder: burn records sum overflows int64."
            );
        }
        burnSum += burn.amount().rawUnits();
    }

    const std::int64_t before = supplyBefore.rawUnits();
    std::int64_t supplyAfterRaw = before;

    if (mintSum > 0 && supplyAfterRaw > INT64_MAX - mintSum) {
        throw std::overflow_error(
            "SupplyDeltaBuilder: supplyBefore + mintedAmount overflows int64."
        );
    }
    supplyAfterRaw += mintSum;

    if (burnSum > supplyAfterRaw) {
        throw std::underflow_error(
            "SupplyDeltaBuilder: burnedAmount exceeds available supply."
        );
    }
    supplyAfterRaw -= burnSum;

    return SupplyDelta(
        blockHeight,
        blockHash,
        epoch,
        supplyBefore,
        utils::Amount::fromRawUnits(mintSum),
        utils::Amount::fromRawUnits(burnSum),
        utils::Amount::fromRawUnits(supplyAfterRaw),
        mintRecords,
        burnRecords
    );
}

} // namespace nodo::economics
