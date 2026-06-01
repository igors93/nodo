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

std::string supplyAuditStatusToString(SupplyAuditStatus status) {
    switch (status) {
        case SupplyAuditStatus::VALID:                    return "VALID";
        case SupplyAuditStatus::INVALID_POLICY:           return "INVALID_POLICY";
        case SupplyAuditStatus::INVALID_DELTA:            return "INVALID_DELTA";
        case SupplyAuditStatus::SUPPLY_CONTINUITY_FAILURE: return "SUPPLY_CONTINUITY_FAILURE";
        case SupplyAuditStatus::SUPPLY_OVERFLOW:          return "SUPPLY_OVERFLOW";
        case SupplyAuditStatus::SUPPLY_UNDERFLOW:         return "SUPPLY_UNDERFLOW";
        default:                                          return "UNKNOWN";
    }
}

SupplySequenceAuditResult::SupplySequenceAuditResult()
    : m_valid(false),
      m_status(SupplyAuditStatus::INVALID_POLICY),
      m_reason(""),
      m_finalSupply(utils::Amount::fromRawUnits(0)),
      m_deltaCount(0),
      m_failedBlockHeight(0) {}

SupplySequenceAuditResult SupplySequenceAuditResult::valid(
    utils::Amount finalSupply,
    std::size_t deltaCount
) {
    SupplySequenceAuditResult r;
    r.m_valid = true;
    r.m_status = SupplyAuditStatus::VALID;
    r.m_finalSupply = finalSupply;
    r.m_deltaCount = deltaCount;
    r.m_failedBlockHeight = 0;
    return r;
}

SupplySequenceAuditResult SupplySequenceAuditResult::invalid(
    SupplyAuditStatus status,
    std::string reason,
    std::uint64_t failedBlockHeight
) {
    SupplySequenceAuditResult r;
    r.m_valid = false;
    r.m_status = status;
    r.m_reason = std::move(reason);
    r.m_failedBlockHeight = failedBlockHeight;
    return r;
}

bool SupplySequenceAuditResult::isValid() const { return m_valid; }
SupplyAuditStatus SupplySequenceAuditResult::status() const { return m_status; }
const std::string& SupplySequenceAuditResult::reason() const { return m_reason; }
utils::Amount SupplySequenceAuditResult::finalSupply() const { return m_finalSupply; }
std::size_t SupplySequenceAuditResult::deltaCount() const { return m_deltaCount; }
std::uint64_t SupplySequenceAuditResult::failedBlockHeight() const { return m_failedBlockHeight; }

std::string SupplySequenceAuditResult::serialize() const {
    std::ostringstream oss;
    oss << "SupplySequenceAuditResult{"
        << "valid=" << (m_valid ? "1" : "0")
        << ";status=" << supplyAuditStatusToString(m_status)
        << ";deltaCount=" << m_deltaCount
        << ";finalSupplyRaw=" << m_finalSupply.rawUnits()
        << ";failedBlockHeight=" << m_failedBlockHeight
        << ";reason=" << m_reason
        << "}";
    return oss.str();
}

SupplySequenceAuditResult SupplyAudit::auditDeltaSequence(
    const MonetaryPolicy& policy,
    const std::vector<SupplyDelta>& deltas
) {
    if (!policy.isValid()) {
        return SupplySequenceAuditResult::invalid(
            SupplyAuditStatus::INVALID_POLICY,
            "MonetaryPolicy is invalid: " + policy.rejectionReason(),
            0
        );
    }

    utils::Amount expectedSupply = policy.initialSupply();

    for (const auto& delta : deltas) {
        if (!delta.isValid()) {
            return SupplySequenceAuditResult::invalid(
                SupplyAuditStatus::INVALID_DELTA,
                "SupplyDelta at block " + std::to_string(delta.blockHeight()) +
                " is invalid: " + delta.rejectionReason(),
                delta.blockHeight()
            );
        }

        if (delta.supplyBefore() != expectedSupply) {
            return SupplySequenceAuditResult::invalid(
                SupplyAuditStatus::SUPPLY_CONTINUITY_FAILURE,
                "Supply continuity failure at block " +
                std::to_string(delta.blockHeight()) +
                ": expected supplyBefore=" +
                std::to_string(expectedSupply.rawUnits()) +
                " but got " +
                std::to_string(delta.supplyBefore().rawUnits()) + ".",
                delta.blockHeight()
            );
        }

        if (delta.supplyAfter().isNegative()) {
            return SupplySequenceAuditResult::invalid(
                SupplyAuditStatus::SUPPLY_UNDERFLOW,
                "Supply underflow at block " +
                std::to_string(delta.blockHeight()) + ".",
                delta.blockHeight()
            );
        }

        expectedSupply = delta.supplyAfter();
    }

    return SupplySequenceAuditResult::valid(expectedSupply, deltas.size());
}

} // namespace nodo::economics
