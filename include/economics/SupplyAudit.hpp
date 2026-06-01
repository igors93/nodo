#ifndef NODO_ECONOMICS_SUPPLY_AUDIT_HPP
#define NODO_ECONOMICS_SUPPLY_AUDIT_HPP

#include "economics/ValidatorStakeState.hpp"
#include "utils/Amount.hpp"

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
};

} // namespace nodo::economics

#endif
