#ifndef NODO_ECONOMICS_MONETARY_FIREWALL_HPP
#define NODO_ECONOMICS_MONETARY_FIREWALL_HPP

#include "economics/MintAuthorization.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDelta.hpp"

#include <string>
#include <vector>

namespace nodo::economics {

/*
 * MonetaryFirewallStatus classifies the outcome of a firewall validation.
 *
 * Security principle:
 * Every rejection must be named precisely so that auditors can trace the
 * cause without inspecting logs. A generic "REJECTED" is forbidden.
 */
enum class MonetaryFirewallStatus {
    ACCEPTED,
    INVALID_POLICY,
    INVALID_SUPPLY_DELTA,
    UNAUTHORIZED_MINT,
    EXPIRED_MINT_AUTHORIZATION,
    MINT_AMOUNT_EXCEEDS_AUTHORIZATION,
    DUPLICATE_MINT_AUTHORIZATION,
    MINT_POLICY_VERSION_MISMATCH,
    SUPPLY_LIMIT_VIOLATION
};

std::string monetaryFirewallStatusToString(MonetaryFirewallStatus status);

class MonetaryFirewallResult {
public:
    MonetaryFirewallResult();

    static MonetaryFirewallResult accepted(const SupplyDelta& delta);
    static MonetaryFirewallResult rejected(
        MonetaryFirewallStatus status,
        std::string reason
    );

    bool isAccepted() const;
    MonetaryFirewallStatus status() const;
    const std::string& reason() const;
    const SupplyDelta& supplyDelta() const;
    std::string serialize() const;

private:
    bool m_accepted;
    MonetaryFirewallStatus m_status;
    std::string m_reason;
    SupplyDelta m_supplyDelta;
};

/*
 * MonetaryFirewallContext carries everything the firewall needs to validate
 * a single block's supply transition.
 *
 * Security principle:
 * The firewall must not read global state. Everything it needs to decide
 * is passed explicitly so the decision is reproducible and auditable.
 */
class MonetaryFirewallContext {
public:
    MonetaryFirewallContext();

    MonetaryFirewallContext(
        MonetaryPolicy policy,
        SupplyDelta supplyDelta,
        std::vector<MintAuthorization> mintAuthorizations
    );

    const MonetaryPolicy& policy() const;
    const SupplyDelta& supplyDelta() const;
    const std::vector<MintAuthorization>& mintAuthorizations() const;

private:
    MonetaryPolicy m_policy;
    SupplyDelta m_supplyDelta;
    std::vector<MintAuthorization> m_mintAuthorizations;
};

/*
 * MonetaryFirewall validates a SupplyDelta against a MonetaryPolicy and a
 * list of MintAuthorization records.
 *
 * Core invariant:
 *   SupplyDelta proves the arithmetic (after = before + minted - burned).
 *   MonetaryFirewall proves the permission (all minted supply is authorized).
 *
 * This is the foundation for Task 03 where the firewall will be wired into
 * the RuntimeBlockPipeline before validator votes.
 *
 * Note on annual inflation:
 * This implementation does not enforce the annual inflation window because
 * that requires epoch/year tracking across blocks. The architecture is ready
 * for it. Do not add a fake check here — leave it for the epoch integration.
 */
class MonetaryFirewall {
public:
    static MonetaryFirewallResult validate(
        const MonetaryFirewallContext& context
    );
};

} // namespace nodo::economics

#endif
