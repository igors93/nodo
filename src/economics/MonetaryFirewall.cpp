#include "economics/MonetaryFirewall.hpp"

#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace nodo::economics {

std::string monetaryFirewallStatusToString(MonetaryFirewallStatus status) {
    switch (status) {
        case MonetaryFirewallStatus::ACCEPTED:
            return "ACCEPTED";
        case MonetaryFirewallStatus::INVALID_POLICY:
            return "INVALID_POLICY";
        case MonetaryFirewallStatus::INVALID_SUPPLY_DELTA:
            return "INVALID_SUPPLY_DELTA";
        case MonetaryFirewallStatus::UNAUTHORIZED_MINT:
            return "UNAUTHORIZED_MINT";
        case MonetaryFirewallStatus::EXPIRED_MINT_AUTHORIZATION:
            return "EXPIRED_MINT_AUTHORIZATION";
        case MonetaryFirewallStatus::MINT_AMOUNT_EXCEEDS_AUTHORIZATION:
            return "MINT_AMOUNT_EXCEEDS_AUTHORIZATION";
        case MonetaryFirewallStatus::DUPLICATE_MINT_AUTHORIZATION:
            return "DUPLICATE_MINT_AUTHORIZATION";
        case MonetaryFirewallStatus::MINT_POLICY_VERSION_MISMATCH:
            return "MINT_POLICY_VERSION_MISMATCH";
        case MonetaryFirewallStatus::SUPPLY_LIMIT_VIOLATION:
            return "SUPPLY_LIMIT_VIOLATION";
        default:
            return "UNKNOWN";
    }
}

MonetaryFirewallResult::MonetaryFirewallResult()
    : m_accepted(false),
      m_status(MonetaryFirewallStatus::INVALID_POLICY),
      m_reason("") {}

MonetaryFirewallResult MonetaryFirewallResult::accepted(const SupplyDelta& delta) {
    MonetaryFirewallResult r;
    r.m_accepted = true;
    r.m_status = MonetaryFirewallStatus::ACCEPTED;
    r.m_reason = "";
    r.m_supplyDelta = delta;
    return r;
}

MonetaryFirewallResult MonetaryFirewallResult::rejected(
    MonetaryFirewallStatus status,
    std::string reason
) {
    MonetaryFirewallResult r;
    r.m_accepted = false;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

bool MonetaryFirewallResult::isAccepted() const { return m_accepted; }
MonetaryFirewallStatus MonetaryFirewallResult::status() const { return m_status; }
const std::string& MonetaryFirewallResult::reason() const { return m_reason; }
const SupplyDelta& MonetaryFirewallResult::supplyDelta() const { return m_supplyDelta; }

std::string MonetaryFirewallResult::serialize() const {
    std::ostringstream oss;
    oss << "MonetaryFirewallResult{"
        << "accepted=" << (m_accepted ? "1" : "0")
        << ";status=" << monetaryFirewallStatusToString(m_status)
        << ";reason=" << m_reason
        << "}";
    return oss.str();
}

MonetaryFirewallContext::MonetaryFirewallContext()
    : m_policy(), m_supplyDelta(), m_mintAuthorizations() {}

MonetaryFirewallContext::MonetaryFirewallContext(
    MonetaryPolicy policy,
    SupplyDelta supplyDelta,
    std::vector<MintAuthorization> mintAuthorizations
)
    : m_policy(std::move(policy)),
      m_supplyDelta(std::move(supplyDelta)),
      m_mintAuthorizations(std::move(mintAuthorizations)) {}

const MonetaryPolicy& MonetaryFirewallContext::policy() const { return m_policy; }
const SupplyDelta& MonetaryFirewallContext::supplyDelta() const { return m_supplyDelta; }
const std::vector<MintAuthorization>& MonetaryFirewallContext::mintAuthorizations() const {
    return m_mintAuthorizations;
}

MonetaryFirewallResult MonetaryFirewall::validate(
    const MonetaryFirewallContext& context
) {
    // Reject invalid policy.
    if (!context.policy().isValid()) {
        return MonetaryFirewallResult::rejected(
            MonetaryFirewallStatus::INVALID_POLICY,
            "MonetaryFirewall: policy is invalid: " +
            context.policy().rejectionReason()
        );
    }

    // Reject invalid supply delta.
    if (!context.supplyDelta().isValid()) {
        return MonetaryFirewallResult::rejected(
            MonetaryFirewallStatus::INVALID_SUPPLY_DELTA,
            "MonetaryFirewall: supply delta is invalid: " +
            context.supplyDelta().rejectionReason()
        );
    }

    // No mints → accept immediately (burn-only or no-op).
    const auto& mintRecords = context.supplyDelta().mintRecords();
    if (mintRecords.empty()) {
        return MonetaryFirewallResult::accepted(context.supplyDelta());
    }

    // Build authorization index and check for duplicate authorizationIds
    //    in the provided authorization list.
    std::map<std::string, const MintAuthorization*> authIndex;
    std::set<std::string> seenAuthIds;

    for (const auto& auth : context.mintAuthorizations()) {
        const std::string& authId = auth.authorizationId();
        if (!seenAuthIds.insert(authId).second) {
            return MonetaryFirewallResult::rejected(
                MonetaryFirewallStatus::DUPLICATE_MINT_AUTHORIZATION,
                "MonetaryFirewall: duplicate MintAuthorization authorizationId='" +
                authId + "'."
            );
        }
        authIndex[authId] = &auth;
    }

    // Validate each mint record and accumulate amounts per authorization.
    const std::uint64_t deltaEpoch = context.supplyDelta().epoch();
    std::map<std::string, std::int64_t> mintedPerAuth;

    for (const auto& mint : mintRecords) {
        const std::string& authId = mint.authorizationId();

        // Find the matching authorization.
        auto it = authIndex.find(authId);
        if (it == authIndex.end()) {
            return MonetaryFirewallResult::rejected(
                MonetaryFirewallStatus::UNAUTHORIZED_MINT,
                "MonetaryFirewall: no MintAuthorization found for authorizationId='" +
                authId + "' (mint id='" + mint.id() + "')."
            );
        }

        const MintAuthorization* auth = it->second;

        // Check authorization validity.
        if (!auth->isValid()) {
            return MonetaryFirewallResult::rejected(
                MonetaryFirewallStatus::UNAUTHORIZED_MINT,
                "MonetaryFirewall: MintAuthorization for authorizationId='" + authId +
                "' is invalid: " + auth->rejectionReason()
            );
        }

        // Check authorization is active at delta epoch.
        if (!auth->isActiveAtEpoch(deltaEpoch)) {
            return MonetaryFirewallResult::rejected(
                MonetaryFirewallStatus::EXPIRED_MINT_AUTHORIZATION,
                "MonetaryFirewall: MintAuthorization authorizationId='" + authId +
                "' is not active at epoch " + std::to_string(deltaEpoch) +
                " (active " + std::to_string(auth->epoch()) +
                " to " + std::to_string(auth->expiresAtEpoch()) + ")."
            );
        }

        // Check policy version matches.
        if (auth->policyVersion() != context.policy().policyVersion()) {
            return MonetaryFirewallResult::rejected(
                MonetaryFirewallStatus::MINT_POLICY_VERSION_MISMATCH,
                "MonetaryFirewall: MintAuthorization authorizationId='" + authId +
                "' policyVersion='" + auth->policyVersion() +
                "' does not match policy policyVersion='" +
                context.policy().policyVersion() + "'."
            );
        }

        // Guard against integer overflow before accumulating. Without this check
        // an attacker could craft N records whose individual amounts are each
        // below maxMintAmount but whose sum wraps around int64_t, producing a
        // negative total that incorrectly passes the limit check below.
        const std::int64_t prevTotal = mintedPerAuth[authId];
        const std::int64_t mintRaw   = mint.amount().rawUnits();
        if (mintRaw > 0 &&
            prevTotal > std::numeric_limits<std::int64_t>::max() - mintRaw) {
            return MonetaryFirewallResult::rejected(
                MonetaryFirewallStatus::MINT_AMOUNT_EXCEEDS_AUTHORIZATION,
                "MonetaryFirewall: accumulated mint amount would overflow for "
                "authorizationId='" + authId + "'."
            );
        }
        mintedPerAuth[authId] = prevTotal + mintRaw;
    }

    // Reject if total minted under any authorization exceeds maxMintAmount.
    for (const auto& [authId, totalMinted] : mintedPerAuth) {
        const MintAuthorization* auth = authIndex.at(authId);
        if (totalMinted > auth->maxMintAmount().rawUnits()) {
            return MonetaryFirewallResult::rejected(
                MonetaryFirewallStatus::MINT_AMOUNT_EXCEEDS_AUTHORIZATION,
                "MonetaryFirewall: minted amount " + std::to_string(totalMinted) +
                " under authorizationId='" + authId + "' exceeds maxMintAmount " +
                std::to_string(auth->maxMintAmount().rawUnits()) + "."
            );
        }
    }

    return MonetaryFirewallResult::accepted(context.supplyDelta());
}

} // namespace nodo::economics
