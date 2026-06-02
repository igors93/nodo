#include "economics/TreasuryExecutionEvidence.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

TreasuryExecutionEvidence::TreasuryExecutionEvidence()
    : m_evidenceId(""),
      m_proposal(),
      m_approval(),
      m_policy(),
      m_treasuryAccountBefore(),
      m_currentBlockHeight(0),
      m_epochSpentSoFar(utils::Amount::fromRawUnits(0)),
      m_spendRecord(),
      m_createdAt(0),
      m_hasGovernanceContext(false),
      m_governanceContext(),
      m_validated(false),
      m_valid(false),
      m_rejectionReason("TreasuryExecutionEvidence: default-constructed, not valid.") {}

TreasuryExecutionEvidence::TreasuryExecutionEvidence(
    std::string evidenceId,
    TreasuryProposal proposal,
    TreasuryApproval approval,
    TreasuryPolicy policy,
    TreasuryAccount treasuryAccountBefore,
    std::uint64_t currentBlockHeight,
    utils::Amount epochSpentSoFar,
    TreasurySpendRecord spendRecord,
    std::int64_t createdAt
)
    : m_evidenceId(std::move(evidenceId)),
      m_proposal(std::move(proposal)),
      m_approval(std::move(approval)),
      m_policy(std::move(policy)),
      m_treasuryAccountBefore(std::move(treasuryAccountBefore)),
      m_currentBlockHeight(currentBlockHeight),
      m_epochSpentSoFar(epochSpentSoFar),
      m_spendRecord(std::move(spendRecord)),
      m_createdAt(createdAt),
      m_hasGovernanceContext(false),
      m_governanceContext(),
      m_validated(false),
      m_valid(false),
      m_rejectionReason("") {}

TreasuryExecutionEvidence::TreasuryExecutionEvidence(
    std::string evidenceId,
    TreasuryProposal proposal,
    TreasuryApproval approval,
    TreasuryPolicy policy,
    TreasuryAccount treasuryAccountBefore,
    std::uint64_t currentBlockHeight,
    utils::Amount epochSpentSoFar,
    TreasurySpendRecord spendRecord,
    std::int64_t createdAt,
    GovernanceApprovalContext governanceContext
)
    : m_evidenceId(std::move(evidenceId)),
      m_proposal(std::move(proposal)),
      m_approval(std::move(approval)),
      m_policy(std::move(policy)),
      m_treasuryAccountBefore(std::move(treasuryAccountBefore)),
      m_currentBlockHeight(currentBlockHeight),
      m_epochSpentSoFar(epochSpentSoFar),
      m_spendRecord(std::move(spendRecord)),
      m_createdAt(createdAt),
      m_hasGovernanceContext(true),
      m_governanceContext(std::move(governanceContext)),
      m_validated(false),
      m_valid(false),
      m_rejectionReason("") {}

const std::string& TreasuryExecutionEvidence::evidenceId() const { return m_evidenceId; }
const TreasuryProposal& TreasuryExecutionEvidence::proposal() const { return m_proposal; }
const TreasuryApproval& TreasuryExecutionEvidence::approval() const { return m_approval; }
const TreasuryPolicy& TreasuryExecutionEvidence::policy() const { return m_policy; }
const TreasuryAccount& TreasuryExecutionEvidence::treasuryAccountBefore() const {
    return m_treasuryAccountBefore;
}
std::uint64_t TreasuryExecutionEvidence::currentBlockHeight() const {
    return m_currentBlockHeight;
}
utils::Amount TreasuryExecutionEvidence::epochSpentSoFar() const { return m_epochSpentSoFar; }
const TreasurySpendRecord& TreasuryExecutionEvidence::spendRecord() const {
    return m_spendRecord;
}
std::int64_t TreasuryExecutionEvidence::createdAt() const { return m_createdAt; }

bool TreasuryExecutionEvidence::hasGovernanceContext() const {
    return m_hasGovernanceContext;
}

const GovernanceApprovalContext& TreasuryExecutionEvidence::governanceContext() const {
    return m_governanceContext;
}

bool TreasuryExecutionEvidence::isValid() const {
    if (!m_validated) {
        validate();
    }
    return m_valid;
}

const std::string& TreasuryExecutionEvidence::rejectionReason() const {
    if (!m_validated) {
        validate();
    }
    return m_rejectionReason;
}

void TreasuryExecutionEvidence::validate() const {
    m_validated = true;

    if (m_evidenceId.empty()) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: evidenceId is empty.";
        return;
    }

    if (m_createdAt <= 0) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: createdAt must be positive.";
        return;
    }

    if (!m_proposal.isValid()) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: proposal is invalid: " +
                            m_proposal.rejectionReason();
        return;
    }

    if (!m_approval.isValid()) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: approval is invalid: " +
                            m_approval.rejectionReason();
        return;
    }

    if (!m_policy.isValid()) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: policy is invalid: " +
                            m_policy.rejectionReason();
        return;
    }

    if (!m_treasuryAccountBefore.isValid()) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: treasuryAccountBefore is invalid: " +
                            m_treasuryAccountBefore.rejectionReason();
        return;
    }

    if (!m_spendRecord.isValid()) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: spendRecord is invalid: " +
                            m_spendRecord.rejectionReason();
        return;
    }

    // Verify spend record is consistent with proposal.
    if (m_spendRecord.proposalId() != m_proposal.proposalId()) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: spendRecord.proposalId='" +
                            m_spendRecord.proposalId() +
                            "' does not match proposal.proposalId='" +
                            m_proposal.proposalId() + "'.";
        return;
    }

    if (m_spendRecord.recipientAddress() != m_proposal.recipientAddress()) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: spendRecord.recipientAddress='" +
                            m_spendRecord.recipientAddress() +
                            "' does not match proposal.recipientAddress='" +
                            m_proposal.recipientAddress() + "'.";
        return;
    }

    if (m_spendRecord.amount() != m_proposal.amount()) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: spendRecord.amount=" +
                            std::to_string(m_spendRecord.amount().rawUnits()) +
                            " does not match proposal.amount=" +
                            std::to_string(m_proposal.amount().rawUnits()) + ".";
        return;
    }

    if (m_spendRecord.purpose() != m_proposal.purpose()) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: spendRecord.purpose='" +
                            m_spendRecord.purpose() +
                            "' does not match proposal.purpose='" +
                            m_proposal.purpose() + "'.";
        return;
    }

    if (m_spendRecord.executedAtBlock() != m_currentBlockHeight) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: spendRecord.executedAtBlock=" +
                            std::to_string(m_spendRecord.executedAtBlock()) +
                            " does not match currentBlockHeight=" +
                            std::to_string(m_currentBlockHeight) + ".";
        return;
    }

    if (m_spendRecord.epoch() != m_proposal.requestedEpoch()) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: spendRecord.epoch=" +
                            std::to_string(m_spendRecord.epoch()) +
                            " does not match proposal.requestedEpoch=" +
                            std::to_string(m_proposal.requestedEpoch()) + ".";
        return;
    }

    // Verify approval is for this proposal.
    if (m_approval.proposalId() != m_proposal.proposalId()) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: approval.proposalId='" +
                            m_approval.proposalId() +
                            "' does not match proposal.proposalId='" +
                            m_proposal.proposalId() + "'.";
        return;
    }

    if (m_hasGovernanceContext &&
        m_governanceContext.governanceLifecycle.proposalEnvelope()
                .treasuryProposal().proposalId() != m_proposal.proposalId()) {
        m_valid = false;
        m_rejectionReason =
            "TreasuryExecutionEvidence: governance lifecycle treasury proposal "
            "does not match evidence proposal.";
        return;
    }

    // Verify treasury balance before matches spend record.
    if (m_spendRecord.treasuryBalanceBefore() != m_treasuryAccountBefore.balance()) {
        m_valid = false;
        m_rejectionReason = "TreasuryExecutionEvidence: spendRecord.treasuryBalanceBefore=" +
                            std::to_string(m_spendRecord.treasuryBalanceBefore().rawUnits()) +
                            " does not match treasuryAccountBefore.balance=" +
                            std::to_string(m_treasuryAccountBefore.balance().rawUnits()) + ".";
        return;
    }

    m_valid = true;
    m_rejectionReason = "";
}

std::string TreasuryExecutionEvidence::serialize() const {
    std::ostringstream oss;
    oss << "TreasuryExecutionEvidence{"
        << "evidenceId=" << m_evidenceId
        << ";currentBlockHeight=" << m_currentBlockHeight
        << ";epochSpentSoFarRawUnits=" << m_epochSpentSoFar.rawUnits()
        << ";createdAt=" << m_createdAt
        << ";hasGovernanceContext=" << (m_hasGovernanceContext ? "1" : "0")
        << ";proposal=" << m_proposal.serialize()
        << ";approval=" << m_approval.serialize()
        << ";policy=" << m_policy.serialize()
        << ";treasuryAccountBefore=" << m_treasuryAccountBefore.serialize()
        << ";spend=" << m_spendRecord.serialize();

    if (m_hasGovernanceContext) {
        oss << ";governanceLifecycle="
            << m_governanceContext.governanceLifecycle.serialize();
    }

    oss << "}";
    return oss.str();
}

} // namespace nodo::economics
