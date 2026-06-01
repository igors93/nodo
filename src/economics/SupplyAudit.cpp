#include "economics/SupplyAudit.hpp"

#include <sstream>

namespace nodo::economics {

SupplyAuditResult::SupplyAuditResult()
    : m_valid(false),
      m_reason("") {}

SupplyAuditResult SupplyAuditResult::valid(
    utils::Amount genesisSupply,
    utils::Amount totalBonded,
    utils::Amount totalSlashed,
    utils::Amount treasury,
    utils::Amount totalRewardsMinted,
    utils::Amount totalFeesBurned,
    utils::Amount freeCirculation
) {
    SupplyAuditResult r;
    r.m_valid = true;
    r.m_genesisSupply = genesisSupply;
    r.m_totalBonded = totalBonded;
    r.m_totalSlashed = totalSlashed;
    r.m_treasury = treasury;
    r.m_totalRewardsMinted = totalRewardsMinted;
    r.m_totalFeesBurned = totalFeesBurned;
    r.m_freeCirculation = freeCirculation;
    return r;
}

SupplyAuditResult SupplyAuditResult::invalid(std::string reason) {
    SupplyAuditResult r;
    r.m_valid = false;
    r.m_reason = std::move(reason);
    return r;
}

bool SupplyAuditResult::isValid() const { return m_valid; }
const std::string& SupplyAuditResult::reason() const { return m_reason; }
utils::Amount SupplyAuditResult::genesisSupply() const { return m_genesisSupply; }
utils::Amount SupplyAuditResult::totalBonded() const { return m_totalBonded; }
utils::Amount SupplyAuditResult::totalSlashed() const { return m_totalSlashed; }
utils::Amount SupplyAuditResult::treasury() const { return m_treasury; }
utils::Amount SupplyAuditResult::totalRewardsMinted() const { return m_totalRewardsMinted; }
utils::Amount SupplyAuditResult::totalFeesBurned() const { return m_totalFeesBurned; }
utils::Amount SupplyAuditResult::freeCirculation() const { return m_freeCirculation; }

std::string SupplyAuditResult::serialize() const {
    std::ostringstream oss;
    oss << "SupplyAuditResult{"
        << "valid=" << (m_valid ? "1" : "0")
        << ";genesisSupply=" << m_genesisSupply.rawUnits()
        << ";bonded=" << m_totalBonded.rawUnits()
        << ";slashed=" << m_totalSlashed.rawUnits()
        << ";treasury=" << m_treasury.rawUnits()
        << ";rewardsMinted=" << m_totalRewardsMinted.rawUnits()
        << ";feesBurned=" << m_totalFeesBurned.rawUnits()
        << ";freeCirculation=" << m_freeCirculation.rawUnits()
        << ";reason=" << m_reason
        << "}";
    return oss.str();
}

SupplyAuditResult SupplyAudit::audit(
    utils::Amount genesisSupply,
    const std::vector<ValidatorStakeState>& stakes,
    utils::Amount treasury,
    utils::Amount totalRewardsMinted,
    utils::Amount totalFeesBurned,
    utils::Amount freeCirculation
) {
    if (genesisSupply.isNegative()) {
        return SupplyAuditResult::invalid("Genesis supply is negative.");
    }
    if (treasury.isNegative()) {
        return SupplyAuditResult::invalid("Treasury amount is negative.");
    }
    if (totalRewardsMinted.isNegative()) {
        return SupplyAuditResult::invalid("Rewards minted is negative.");
    }
    if (totalFeesBurned.isNegative()) {
        return SupplyAuditResult::invalid("Fees burned is negative.");
    }
    if (freeCirculation.isNegative()) {
        return SupplyAuditResult::invalid("Free circulation is negative.");
    }

    utils::Amount totalBonded = utils::Amount::fromRawUnits(0);
    utils::Amount totalSlashed = utils::Amount::fromRawUnits(0);

    for (const auto& stake : stakes) {
        if (!stake.isValid()) {
            return SupplyAuditResult::invalid(
                "Invalid stake state for validator: " +
                stake.account().validatorAddress()
            );
        }
        totalBonded = totalBonded + stake.account().bondedAmount();
        totalSlashed = totalSlashed + stake.account().slashedAmount();
    }

    // Invariant: genesisSupply + rewardsMinted - feesBurned
    //            = bonded + freeCirculation + treasury + slashed
    const utils::Amount lhs = (genesisSupply + totalRewardsMinted) - totalFeesBurned;
    const utils::Amount rhs = totalBonded + freeCirculation + treasury + totalSlashed;

    if (lhs.isNegative()) {
        return SupplyAuditResult::invalid(
            "Effective supply (genesis + rewards - burned) is negative."
        );
    }

    if (lhs != rhs) {
        std::ostringstream oss;
        oss << "Supply invariant violated: "
            << "lhs=" << lhs.rawUnits()
            << " != rhs=" << rhs.rawUnits()
            << " (bonded=" << totalBonded.rawUnits()
            << " free=" << freeCirculation.rawUnits()
            << " treasury=" << treasury.rawUnits()
            << " slashed=" << totalSlashed.rawUnits() << ")";
        return SupplyAuditResult::invalid(oss.str());
    }

    return SupplyAuditResult::valid(
        genesisSupply,
        totalBonded,
        totalSlashed,
        treasury,
        totalRewardsMinted,
        totalFeesBurned,
        freeCirculation
    );
}

} // namespace nodo::economics
