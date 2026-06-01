#include "economics/MonetaryValidationGate.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

std::string monetaryValidationGateStatusToString(MonetaryValidationGateStatus status) {
    switch (status) {
        case MonetaryValidationGateStatus::ACCEPTED:
            return "ACCEPTED";
        case MonetaryValidationGateStatus::REJECTED_BY_FIREWALL:
            return "REJECTED_BY_FIREWALL";
        default:
            return "UNKNOWN";
    }
}

MonetaryValidationGateResult::MonetaryValidationGateResult()
    : m_accepted(false),
      m_status(MonetaryValidationGateStatus::REJECTED_BY_FIREWALL),
      m_firewallStatus(MonetaryFirewallStatus::INVALID_POLICY),
      m_reason("") {}

MonetaryValidationGateResult MonetaryValidationGateResult::accepted() {
    MonetaryValidationGateResult r;
    r.m_accepted = true;
    r.m_status = MonetaryValidationGateStatus::ACCEPTED;
    r.m_firewallStatus = MonetaryFirewallStatus::ACCEPTED;
    r.m_reason = "";
    return r;
}

MonetaryValidationGateResult MonetaryValidationGateResult::rejected(
    MonetaryFirewallStatus firewallStatus,
    std::string reason
) {
    MonetaryValidationGateResult r;
    r.m_accepted = false;
    r.m_status = MonetaryValidationGateStatus::REJECTED_BY_FIREWALL;
    r.m_firewallStatus = firewallStatus;
    r.m_reason = std::move(reason);
    return r;
}

bool MonetaryValidationGateResult::isAccepted() const { return m_accepted; }
MonetaryValidationGateStatus MonetaryValidationGateResult::status() const { return m_status; }
MonetaryFirewallStatus MonetaryValidationGateResult::firewallStatus() const { return m_firewallStatus; }
const std::string& MonetaryValidationGateResult::reason() const { return m_reason; }

std::string MonetaryValidationGateResult::serialize() const {
    std::ostringstream oss;
    oss << "MonetaryValidationGateResult{"
        << "accepted=" << (m_accepted ? "1" : "0")
        << ";status=" << monetaryValidationGateStatusToString(m_status)
        << ";firewallStatus=" << monetaryFirewallStatusToString(m_firewallStatus)
        << ";reason=" << m_reason
        << "}";
    return oss.str();
}

MonetaryValidationGateResult MonetaryValidationGate::validate(
    const MonetaryPolicy& policy,
    const SupplyDelta& delta,
    const std::vector<MintAuthorization>& authorizations
) {
    const MonetaryFirewallContext ctx(policy, delta, authorizations);
    const MonetaryFirewallResult firewallResult = MonetaryFirewall::validate(ctx);

    if (!firewallResult.isAccepted()) {
        return MonetaryValidationGateResult::rejected(
            firewallResult.status(),
            firewallResult.reason()
        );
    }

    return MonetaryValidationGateResult::accepted();
}

} // namespace nodo::economics
