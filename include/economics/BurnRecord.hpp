#ifndef NODO_ECONOMICS_BURN_RECORD_HPP
#define NODO_ECONOMICS_BURN_RECORD_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * BurnType classifies the reason coins were permanently removed from supply.
 *
 * Security principle:
 * No coin can be burned without an explicit burn type. This prevents the
 * economic accounting layer from silently disappearing supply.
 */
enum class BurnType {
    FEE_BURN,
    SLASH_BURN,
    VOLUNTARY_BURN,
    GOVERNANCE_DEPOSIT_BURN,
    PENALTY_BURN
};

std::string burnTypeToString(BurnType burnType);
BurnType burnTypeFromString(const std::string& value);

/*
 * BurnRecord records coins permanently removed from circulating supply.
 *
 * Core principle:
 * Every supply reduction must have a BurnRecord with an explicit reason,
 * source address, and burn type. A burn without these fields is rejected.
 */
class BurnRecord {
public:
    BurnRecord();

    BurnRecord(
        std::string burnId,
        std::uint64_t blockHeight,
        std::uint64_t epoch,
        std::string sourceAddress,
        utils::Amount amount,
        std::string reason,
        BurnType burnType
    );

    const std::string& burnId() const;
    std::uint64_t blockHeight() const;
    std::uint64_t epoch() const;
    const std::string& sourceAddress() const;
    utils::Amount amount() const;
    const std::string& reason() const;
    BurnType burnType() const;

    bool isValid() const;
    std::string rejectionReason() const;
    std::string serialize() const;

private:
    std::string m_burnId;
    std::uint64_t m_blockHeight;
    std::uint64_t m_epoch;
    std::string m_sourceAddress;
    utils::Amount m_amount;
    std::string m_reason;
    BurnType m_burnType;
};

} // namespace nodo::economics

#endif
