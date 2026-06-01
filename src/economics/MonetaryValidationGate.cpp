#include "economics/MonetaryValidationGate.hpp"

#include "economics/MonetaryFirewall.hpp"

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
      m_reason("") {}

MonetaryValidationGateResult MonetaryValidationGateResult::accepted() {
    MonetaryValidationGateResult r;
    r.m_accepted = true;
    r.m_status = MonetaryValidationGateStatus::ACCEPTED;
    r.m_reason = "";
    return r;
}

MonetaryValidationGateResult MonetaryValidationGateResult::rejected(std::string reason) {
    MonetaryValidationGateResult r;
    r.m_accepted = false;
    r.m_status = MonetaryValidationGateStatus::REJECTED_BY_FIREWALL;
    r.m_reason = std::move(reason);
    return r;
}

bool MonetaryValidationGateResult::isAccepted() const { return m_accepted; }
MonetaryValidationGateStatus MonetaryValidationGateResult::status() const { return m_status; }
const std::string& MonetaryValidationGateResult::reason() const { return m_reason; }

std::string MonetaryValidationGateResult::serialize() const {
    std::ostringstream oss;
    oss << "MonetaryValidationGateResult{"
        << "accepted=" << (m_accepted ? "1" : "0")
        << ";status=" << monetaryValidationGateStatusToString(m_status)
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
        return MonetaryValidationGateResult::rejected(firewallResult.reason());
    }

    return MonetaryValidationGateResult::accepted();

}

} // namespace nodo::economics
