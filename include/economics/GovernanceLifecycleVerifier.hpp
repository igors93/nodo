#ifndef NODO_ECONOMICS_GOVERNANCE_LIFECYCLE_VERIFIER_HPP
#define NODO_ECONOMICS_GOVERNANCE_LIFECYCLE_VERIFIER_HPP

#include "economics/GovernanceLifecycleRecord.hpp"

#include <string>

namespace nodo::economics {

enum class GovernanceLifecycleVerificationStatus {
    VERIFIED,
    INVALID_LIFECYCLE,
    VOTE_AUDIT_FAILED,
    TALLY_MISMATCH,
    DECISION_AUDIT_FAILED
};

std::string governanceLifecycleVerificationStatusToString(
    GovernanceLifecycleVerificationStatus status
);

class GovernanceLifecycleVerificationResult {
public:
    GovernanceLifecycleVerificationResult();

    static GovernanceLifecycleVerificationResult accepted();

    static GovernanceLifecycleVerificationResult rejected(
        GovernanceLifecycleVerificationStatus status,
        std::string reason
    );

    bool verified() const;
    GovernanceLifecycleVerificationStatus status() const;
    const std::string& reason() const;

private:
    bool m_verified;
    GovernanceLifecycleVerificationStatus m_status;
    std::string m_reason;
};

class GovernanceLifecycleVerifier {
public:
    static GovernanceLifecycleVerificationResult verify(
        const GovernanceLifecycleRecord& lifecycle
    );
};

} // namespace nodo::economics

#endif
