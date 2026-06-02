#ifndef NODO_ECONOMICS_GOVERNANCE_PROPOSAL_ENVELOPE_HPP
#define NODO_ECONOMICS_GOVERNANCE_PROPOSAL_ENVELOPE_HPP

#include "economics/TreasuryProposal.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * GovernanceProposalEnvelope wraps a TreasuryProposal with governance
 * lifecycle metadata: who submitted it, when, under which policy version,
 * and a summary hash binding the submission content.
 *
 * Security principle:
 * A TreasuryProposal must enter the governance lifecycle through an envelope
 * before it can produce a TreasuryApproval. The envelope ties the proposal
 * to a specific governance policy version, preventing policy substitution.
 */
class GovernanceProposalEnvelope {
public:
    GovernanceProposalEnvelope();

    GovernanceProposalEnvelope(
        std::string governanceProposalId,
        std::string proposalType,
        TreasuryProposal treasuryProposal,
        std::uint64_t submittedAtBlock,
        std::string submittedBy,
        std::string governancePolicyVersion,
        std::string summaryHash
    );

    const std::string& governanceProposalId() const;
    const std::string& proposalType() const;
    const TreasuryProposal& treasuryProposal() const;
    std::uint64_t submittedAtBlock() const;
    const std::string& submittedBy() const;
    const std::string& governancePolicyVersion() const;
    const std::string& summaryHash() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_governanceProposalId;
    std::string m_proposalType;
    TreasuryProposal m_treasuryProposal;
    std::uint64_t m_submittedAtBlock;
    std::string m_submittedBy;
    std::string m_governancePolicyVersion;
    std::string m_summaryHash;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
