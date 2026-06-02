#ifndef NODO_ECONOMICS_TREASURY_EXECUTION_VALIDATOR_HPP
#define NODO_ECONOMICS_TREASURY_EXECUTION_VALIDATOR_HPP

#include "economics/TreasuryExecutionEvidence.hpp"

#include <string>

namespace nodo::economics {

enum class TreasuryExecutionValidationStatus {
    ACCEPTED,
    INVALID_EVIDENCE,
    SPEND_VALIDATOR_REJECTED,
    SPEND_RECORD_MISMATCH,
    MISSING_GOVERNANCE_CONTEXT,
    INVALID_GOVERNANCE_CONTEXT
};

std::string treasuryExecutionValidationStatusToString(
    TreasuryExecutionValidationStatus status
);

class TreasuryExecutionValidationResult {
public:
    TreasuryExecutionValidationResult();

    static TreasuryExecutionValidationResult accepted();
    static TreasuryExecutionValidationResult rejected(
        TreasuryExecutionValidationStatus status,
        std::string reason
    );

    bool isAccepted() const;
    TreasuryExecutionValidationStatus status() const;
    const std::string& reason() const;

private:
    TreasuryExecutionValidationStatus m_status;
    std::string m_reason;
};

/*
 * TreasuryExecutionValidator recomputes the treasury spend from the evidence
 * using TreasurySpendValidator and verifies that the recomputed spend record
 * matches the spend record stored in the evidence.
 *
 * Security principle:
 * Storing a spend record inside evidence is not sufficient. The validator must
 * be able to independently reproduce the spend from the proposal, approval,
 * policy, and treasury state and confirm that it produces exactly the same
 * spendRecord. Any divergence means the evidence was tampered with.
 */
class TreasuryExecutionValidator {
public:
    static TreasuryExecutionValidationResult validateEvidence(
        const TreasuryExecutionEvidence& evidence
    );
};

} // namespace nodo::economics

#endif
