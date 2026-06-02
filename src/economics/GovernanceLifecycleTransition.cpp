#include "economics/GovernanceLifecycleTransition.hpp"

#include "economics/GovernanceTransitionProof.hpp"

#include <utility>

namespace nodo::economics {

GovernanceLifecycleTransition::GovernanceLifecycleTransition()
    : m_fromState(GovernanceLifecycleState::DRAFT),
      m_toState(GovernanceLifecycleState::DRAFT),
      m_transitionBlock(0),
      m_valid(false),
      m_rejectionReason("GovernanceLifecycleTransition: default-constructed.") {}

GovernanceLifecycleTransition::GovernanceLifecycleTransition(
    std::string transitionId,
    std::string governanceProposalId,
    GovernanceLifecycleState fromState,
    GovernanceLifecycleState toState,
    std::uint64_t transitionBlock,
    std::string actorId,
    std::string reason,
    std::string transitionProof,
    std::string policyVersion
)
    : m_transitionId(std::move(transitionId)),
      m_governanceProposalId(std::move(governanceProposalId)),
      m_fromState(fromState),
      m_toState(toState),
      m_transitionBlock(transitionBlock),
      m_actorId(std::move(actorId)),
      m_reason(std::move(reason)),
      m_transitionProof(std::move(transitionProof)),
      m_policyVersion(std::move(policyVersion)),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_transitionId.empty()) {
        m_rejectionReason =
            "GovernanceLifecycleTransition: transitionId must not be empty.";
        return;
    }

    if (m_governanceProposalId.empty()) {
        m_rejectionReason =
            "GovernanceLifecycleTransition: governanceProposalId must not be empty.";
        return;
    }

    if (m_actorId.empty()) {
        m_rejectionReason =
            "GovernanceLifecycleTransition: actorId must not be empty.";
        return;
    }

    if (m_transitionProof.empty()) {
        m_rejectionReason =
            "GovernanceLifecycleTransition: transitionProof must not be empty.";
        return;
    }

    if (m_policyVersion.empty()) {
        m_rejectionReason =
            "GovernanceLifecycleTransition: policyVersion must not be empty.";
        return;
    }

    // CANCELLED and EXPIRED transitions require a non-empty reason.
    if ((m_toState == GovernanceLifecycleState::CANCELLED ||
         m_toState == GovernanceLifecycleState::EXPIRED) &&
        m_reason.empty()) {
        m_rejectionReason =
            "GovernanceLifecycleTransition: reason is required for CANCELLED or EXPIRED transitions.";
        return;
    }

    // Verify proof covers all fields deterministically.
    if (!GovernanceTransitionProof::verify(
            m_governanceProposalId,
            m_transitionId,
            m_fromState,
            m_toState,
            m_transitionBlock,
            m_actorId,
            m_policyVersion,
            m_transitionProof)) {
        m_rejectionReason =
            "GovernanceLifecycleTransition: transitionProof does not match fields.";
        return;
    }

    m_valid = true;
}

const std::string& GovernanceLifecycleTransition::transitionId() const {
    return m_transitionId;
}
const std::string& GovernanceLifecycleTransition::governanceProposalId() const {
    return m_governanceProposalId;
}
GovernanceLifecycleState GovernanceLifecycleTransition::fromState() const {
    return m_fromState;
}
GovernanceLifecycleState GovernanceLifecycleTransition::toState() const {
    return m_toState;
}
std::uint64_t GovernanceLifecycleTransition::transitionBlock() const {
    return m_transitionBlock;
}
const std::string& GovernanceLifecycleTransition::actorId() const {
    return m_actorId;
}
const std::string& GovernanceLifecycleTransition::reason() const {
    return m_reason;
}
const std::string& GovernanceLifecycleTransition::transitionProof() const {
    return m_transitionProof;
}
const std::string& GovernanceLifecycleTransition::policyVersion() const {
    return m_policyVersion;
}
bool GovernanceLifecycleTransition::isValid() const {
    return m_valid;
}
const std::string& GovernanceLifecycleTransition::rejectionReason() const {
    return m_rejectionReason;
}

} // namespace nodo::economics
