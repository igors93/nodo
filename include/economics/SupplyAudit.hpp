#ifndef NODO_ECONOMICS_SUPPLY_AUDIT_HPP
#define NODO_ECONOMICS_SUPPLY_AUDIT_HPP

#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDelta.hpp"
#include "economics/ValidatorStakeState.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::economics {

class SupplyAuditResult {
public:
    SupplyAuditResult();

    static SupplyAuditResult valid(
        utils::Amount genesisSupply,
        utils::Amount totalBonded,
        utils::Amount totalSlashed,
        utils::Amount treasury,
        utils::Amount totalRewardsMinted,
        utils::Amount totalFeesBurned,
        utils::Amount freeCirculation
    );

    static SupplyAuditResult invalid(std::string reason);

    bool isValid() const;
    const std::string& reason() const;

    utils::Amount genesisSupply() const;
    utils::Amount totalBonded() const;
    utils::Amount totalSlashed() const;
    utils::Amount treasury() const;
    utils::Amount totalRewardsMinted() const;
    utils::Amount totalFeesBurned() const;
    utils::Amount freeCirculation() const;

    std::string serialize() const;

private:
    bool m_valid;
    std::string m_reason;
    utils::Amount m_genesisSupply;
    utils::Amount m_totalBonded;
    utils::Amount m_totalSlashed;
    utils::Amount m_treasury;
    utils::Amount m_totalRewardsMinted;
    utils::Amount m_totalFeesBurned;
    utils::Amount m_freeCirculation;
};

enum class SupplyAuditStatus {
    VALID,
    INVALID_POLICY,
    INVALID_DELTA,
    SUPPLY_CONTINUITY_FAILURE,
    SUPPLY_OVERFLOW,
    SUPPLY_UNDERFLOW
};

std::string supplyAuditStatusToString(SupplyAuditStatus status);

/*
 * SupplySequenceAuditResult is the outcome of auditing a sequence of
 * SupplyDelta records against a MonetaryPolicy.
 */
class SupplySequenceAuditResult {
public:
    SupplySequenceAuditResult();

    static SupplySequenceAuditResult valid(
        utils::Amount finalSupply,
        std::size_t deltaCount
    );

    static SupplySequenceAuditResult invalid(
        SupplyAuditStatus status,
        std::string reason,
        std::uint64_t failedBlockHeight
    );

    bool isValid() const;
    SupplyAuditStatus status() const;
    const std::string& reason() const;
    utils::Amount finalSupply() const;
    std::size_t deltaCount() const;
    std::uint64_t failedBlockHeight() const;

    std::string serialize() const;

private:
    bool m_valid;
    SupplyAuditStatus m_status;
    std::string m_reason;
    utils::Amount m_finalSupply;
    std::size_t m_deltaCount;
    std::uint64_t m_failedBlockHeight;
};

/*
 * SupplyAudit verifies that the total token supply is internally consistent.
 *
 * Security principle:
 * Supply must satisfy the invariant:
 *   genesisSupply + rewardsMinted - feesBurned
 *     = bonded + free + treasury + slashed
 *
 * Any deviation indicates a bug in the economic accounting layer and must be
 * treated as a protocol invariant violation.
 */
class SupplyAudit {
public:
    static SupplyAuditResult audit(
        utils::Amount genesisSupply,
        const std::vector<ValidatorStakeState>& stakes,
        utils::Amount treasury,
        utils::Amount totalRewardsMinted,
        utils::Amount totalFeesBurned,
        utils::Amount freeCirculation
    );

    static SupplySequenceAuditResult auditDeltaSequence(
        const MonetaryPolicy& policy,
        const std::vector<SupplyDelta>& deltas
    );
};

} // namespace nodo::economics

#endif
