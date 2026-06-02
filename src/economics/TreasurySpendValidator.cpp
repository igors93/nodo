#include "economics/TreasurySpendValidator.hpp"

#include <sstream>
#include <utility>

namespace nodo::economics {

std::string treasurySpendStatusToString(TreasurySpendStatus status) {
    switch (status) {
        case TreasurySpendStatus::ACCEPTED:
            return "ACCEPTED";
        case TreasurySpendStatus::INVALID_TREASURY:
            return "INVALID_TREASURY";
        case TreasurySpendStatus::INVALID_POLICY:
            return "INVALID_POLICY";
        case TreasurySpendStatus::INVALID_PROPOSAL:
            return "INVALID_PROPOSAL";
        case TreasurySpendStatus::INVALID_APPROVAL:
            return "INVALID_APPROVAL";
        case TreasurySpendStatus::APPROVAL_MISMATCH:
            return "APPROVAL_MISMATCH";
        case TreasurySpendStatus::TIMELOCK_NOT_SATISFIED:
            return "TIMELOCK_NOT_SATISFIED";
        case TreasurySpendStatus::INSUFFICIENT_TREASURY_BALANCE:
            return "INSUFFICIENT_TREASURY_BALANCE";
        case TreasurySpendStatus::PROPOSAL_LIMIT_EXCEEDED:
            return "PROPOSAL_LIMIT_EXCEEDED";
        case TreasurySpendStatus::EPOCH_LIMIT_EXCEEDED:
            return "EPOCH_LIMIT_EXCEEDED";
        case TreasurySpendStatus::TREASURY_LOCKED:
            return "TREASURY_LOCKED";
        default:
            return "UNKNOWN";
    }
}

TreasurySpendValidationResult::TreasurySpendValidationResult()
    : m_status(TreasurySpendStatus::INVALID_TREASURY),
      m_reason("Uninitialized."),
      m_spendRecord() {}

TreasurySpendValidationResult TreasurySpendValidationResult::accepted(
    TreasurySpendRecord spendRecord
) {
    TreasurySpendValidationResult r;
    r.m_status = TreasurySpendStatus::ACCEPTED;
    r.m_reason = "";
    r.m_spendRecord = std::move(spendRecord);
    return r;
}

TreasurySpendValidationResult TreasurySpendValidationResult::rejected(
    TreasurySpendStatus status,
    std::string reason
) {
    TreasurySpendValidationResult r;
    r.m_status = status;
    r.m_reason = std::move(reason);
    return r;
}

bool TreasurySpendValidationResult::accepted() const {
    return m_status == TreasurySpendStatus::ACCEPTED;
}

TreasurySpendStatus TreasurySpendValidationResult::status() const {
    return m_status;
}

const std::string& TreasurySpendValidationResult::reason() const {
    return m_reason;
}

const TreasurySpendRecord& TreasurySpendValidationResult::spendRecord() const {
    return m_spendRecord;
}

std::string TreasurySpendValidationResult::serialize() const {
    std::ostringstream oss;
    oss << "TreasurySpendValidationResult{"
        << "status=" << treasurySpendStatusToString(m_status)
        << ";reason=" << m_reason
        << "}";
    return oss.str();
}

TreasurySpendValidationResult TreasurySpendValidator::validateSpend(
    const TreasuryAccount& treasury,
    const TreasuryPolicy& policy,
    const TreasuryProposal& proposal,
    const TreasuryApproval& approval,
    std::uint64_t currentBlockHeight,
    utils::Amount epochSpentSoFar
) {
    if (!treasury.isValid()) {
        return TreasurySpendValidationResult::rejected(
            TreasurySpendStatus::INVALID_TREASURY,
            "treasury account is invalid: " + treasury.rejectionReason()
        );
    }

    if (!policy.isValid()) {
        return TreasurySpendValidationResult::rejected(
            TreasurySpendStatus::INVALID_POLICY,
            "treasury policy is invalid: " + policy.rejectionReason()
        );
    }

    if (!proposal.isValid()) {
        return TreasurySpendValidationResult::rejected(
            TreasurySpendStatus::INVALID_PROPOSAL,
            "treasury proposal is invalid: " + proposal.rejectionReason()
        );
    }

    if (policy.requireApproval()) {
        if (!approval.isValid()) {
            return TreasurySpendValidationResult::rejected(
                TreasurySpendStatus::INVALID_APPROVAL,
                "approval required but approval is invalid: " + approval.rejectionReason()
            );
        }

        if (approval.proposalId() != proposal.proposalId()) {
            return TreasurySpendValidationResult::rejected(
                TreasurySpendStatus::APPROVAL_MISMATCH,
                "approval.proposalId (" + approval.proposalId() +
                ") does not match proposal.proposalId (" + proposal.proposalId() + ")"
            );
        }
    }

    // Timelock: currentBlockHeight >= proposal.createdAtBlock + policy.timelockBlocks.
    const std::uint64_t unlockHeight =
        proposal.createdAtBlock() + policy.timelockBlocks();
    if (currentBlockHeight < unlockHeight) {
        return TreasurySpendValidationResult::rejected(
            TreasurySpendStatus::TIMELOCK_NOT_SATISFIED,
            "timelock not satisfied: currentBlock=" + std::to_string(currentBlockHeight) +
            " unlockAt=" + std::to_string(unlockHeight)
        );
    }

    // Treasury balance check.
    if (proposal.amount() > treasury.balance()) {
        return TreasurySpendValidationResult::rejected(
            TreasurySpendStatus::INSUFFICIENT_TREASURY_BALANCE,
            "proposal amount (" + std::to_string(proposal.amount().rawUnits()) +
            ") exceeds treasury balance (" + std::to_string(treasury.balance().rawUnits()) + ")"
        );
    }

    // Per-proposal limit.
    if (policy.maxSpendPerProposal().isPositive() &&
        proposal.amount() > policy.maxSpendPerProposal()) {
        return TreasurySpendValidationResult::rejected(
            TreasurySpendStatus::PROPOSAL_LIMIT_EXCEEDED,
            "proposal amount (" + std::to_string(proposal.amount().rawUnits()) +
            ") exceeds maxSpendPerProposal (" +
            std::to_string(policy.maxSpendPerProposal().rawUnits()) + ")"
        );
    }

    // Per-epoch limit.
    if (policy.maxSpendPerEpoch().isPositive()) {
        const std::int64_t totalAfter =
            epochSpentSoFar.rawUnits() + proposal.amount().rawUnits();
        if (utils::Amount::fromRawUnits(totalAfter) > policy.maxSpendPerEpoch()) {
            return TreasurySpendValidationResult::rejected(
                TreasurySpendStatus::EPOCH_LIMIT_EXCEEDED,
                "epoch spend total (" + std::to_string(totalAfter) +
                ") would exceed maxSpendPerEpoch (" +
                std::to_string(policy.maxSpendPerEpoch().rawUnits()) + ")"
            );
        }
    }

    // Lock check — after balance/limit checks so we can give a specific reason.
    if (treasury.isLocked() && !policy.allowSpendingWhenLocked()) {
        return TreasurySpendValidationResult::rejected(
            TreasurySpendStatus::TREASURY_LOCKED,
            "treasury is locked and policy does not allow spending when locked: " +
            treasury.lockReason()
        );
    }

    const utils::Amount balanceBefore = treasury.balance();
    const utils::Amount balanceAfter  = utils::Amount::fromRawUnits(
        balanceBefore.rawUnits() - proposal.amount().rawUnits()
    );

    const TreasurySpendRecord spendRecord(
        "spend-" + proposal.proposalId(),
        proposal.proposalId(),
        proposal.recipientAddress(),
        proposal.amount(),
        proposal.purpose(),
        currentBlockHeight,
        proposal.requestedEpoch(),
        balanceBefore,
        balanceAfter
    );

    if (!spendRecord.isValid()) {
        return TreasurySpendValidationResult::rejected(
            TreasurySpendStatus::INVALID_PROPOSAL,
            "spend record construction failed: " + spendRecord.rejectionReason()
        );
    }

    return TreasurySpendValidationResult::accepted(spendRecord);
}

} // namespace nodo::economics
