#ifndef NODO_ECONOMICS_MONETARY_VALIDATION_GATE_HPP
#define NODO_ECONOMICS_MONETARY_VALIDATION_GATE_HPP

#include "economics/MintAuthorization.hpp"
#include "economics/MonetaryFirewall.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDelta.hpp"

#include <string>
#include <vector>

namespace nodo::economics {

/*
 * MonetaryValidationGateStatus is the pipeline-facing outcome of monetary
 * validation for a single block.
 */
enum class MonetaryValidationGateStatus {
    ACCEPTED,
    REJECTED_BY_FIREWALL
};

std::string monetaryValidationGateStatusToString(MonetaryValidationGateStatus status);

/*
 * MonetaryValidationGateResult carries the full outcome of gate validation.
 *
 * isAccepted()    — true iff the gate passed.
 * status()        — ACCEPTED or REJECTED_BY_FIREWALL.
 * firewallStatus()— the underlying MonetaryFirewallStatus for detailed routing.
 *                   Returns ACCEPTED when the gate accepted.
 * reason()        — the rejection message when rejected.
 */
class MonetaryValidationGateResult {
public:
    MonetaryValidationGateResult();

    static MonetaryValidationGateResult accepted();

    static MonetaryValidationGateResult rejected(
        MonetaryFirewallStatus firewallStatus,
        std::string reason
    );

    bool isAccepted() const;
    MonetaryValidationGateStatus status() const;
    MonetaryFirewallStatus firewallStatus() const;
    const std::string& reason() const;

    std::string serialize() const;

private:
    bool m_accepted;
    MonetaryValidationGateStatus m_status;
    MonetaryFirewallStatus m_firewallStatus;
    std::string m_reason;
};

/*
 * MonetaryValidationGate is the pipeline-facing interface for monetary
 * validation.
 *
 * Separation of concerns:
 *   MonetaryFirewall        — economics logic (arithmetic + authorization rules).
 *   MonetaryValidationGate  — runtime interface the pipeline calls before votes.
 *
 * RuntimeBlockPipeline calls this before buildValidatorVotes.
 */
class MonetaryValidationGate {
public:
    static MonetaryValidationGateResult validate(
        const MonetaryPolicy& policy,
        const SupplyDelta& delta,
        const std::vector<MintAuthorization>& authorizations
    );
};

} // namespace nodo::economics

#endif
