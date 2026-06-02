#ifndef NODO_ECONOMICS_TREASURY_SPEND_VALIDATOR_HPP
#define NODO_ECONOMICS_TREASURY_SPEND_VALIDATOR_HPP

#include "economics/TreasuryAccount.hpp"
#include "economics/TreasuryApproval.hpp"
#include "economics/TreasuryPolicy.hpp"
#include "economics/TreasuryProposal.hpp"
#include "economics/TreasurySpendRecord.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::economics {

enum class TreasurySpendStatus {
    ACCEPTED,
    INVALID_TREASURY,
    INVALID_POLICY,
    INVALID_PROPOSAL,
    INVALID_APPROVAL,
    APPROVAL_MISMATCH,
    TIMELOCK_NOT_SATISFIED,
    INSUFFICIENT_TREASURY_BALANCE,
    PROPOSAL_LIMIT_EXCEEDED,
    EPOCH_LIMIT_EXCEEDED,
    TREASURY_LOCKED
};

std::string treasurySpendStatusToString(TreasurySpendStatus status);

class TreasurySpendValidationResult {
public:
    TreasurySpendValidationResult();

    static TreasurySpendValidationResult accepted(TreasurySpendRecord spendRecord);

    static TreasurySpendValidationResult rejected(
        TreasurySpendStatus status,
        std::string reason
    );

    bool accepted() const;
    TreasurySpendStatus status() const;
    const std::string& reason() const;
    const TreasurySpendRecord& spendRecord() const;

    std::string serialize() const;

private:
    TreasurySpendStatus m_status;
    std::string m_reason;
    TreasurySpendRecord m_spendRecord;
};

/*
 * TreasurySpendValidator enforces the full authorization chain for treasury
 * spending: valid treasury, valid policy, valid proposal, valid approval (if
 * required), timelock, balance limits, epoch limits, and lock status.
 *
 * Security principle:
 * No treasury spend may succeed without passing every check. The validator
 * returns a TreasurySpendRecord only on ACCEPTED. All other outcomes are
 * explicit rejections with a status code.
 */
class TreasurySpendValidator {
public:
    static TreasurySpendValidationResult validateSpend(
        const TreasuryAccount& treasury,
        const TreasuryPolicy& policy,
        const TreasuryProposal& proposal,
        const TreasuryApproval& approval,
        std::uint64_t currentBlockHeight,
        utils::Amount epochSpentSoFar
    );
};

} // namespace nodo::economics

#endif
