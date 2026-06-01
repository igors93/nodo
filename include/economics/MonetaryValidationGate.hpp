#ifndef NODO_ECONOMICS_MONETARY_VALIDATION_GATE_HPP
#define NODO_ECONOMICS_MONETARY_VALIDATION_GATE_HPP

#include "economics/MintAuthorization.hpp"
#include "economics/MonetaryPolicy.hpp"
#include "economics/SupplyDelta.hpp"

#include <string>
#include <vector>

namespace nodo::economics {

/*
 * MonetaryValidationGateStatus is the pipeline-facing outcome of monetary
 * validation for a single block.
 *
 * Task 04 will use this status to gate validator votes: a block may not receive
 * votes until the gate returns ACCEPTED.
 */
enum class MonetaryValidationGateStatus {
    ACCEPTED,
    REJECTED_BY_FIREWALL
};

std::string monetaryValidationGateStatusToString(MonetaryValidationGateStatus status);

/*
 * MonetaryValidationGateResult carries the full outcome of gate validation.
 *
 * accepted()  == true iff the gate passed.
 * reason()    carries the firewall rejection message when rejected.
 * status()    identifies the category of rejection for pipeline routing.
 */
class MonetaryValidationGateResult {
public:
    MonetaryValidationGateResult();

    static MonetaryValidationGateResult accepted();
    static MonetaryValidationGateResult rejected(std::string reason);

    bool isAccepted() const;
    MonetaryValidationGateStatus status() const;
    const std::string& reason() const;

    std::string serialize() const;

private:
    bool m_accepted;
    MonetaryValidationGateStatus m_status;
    std::string m_reason;
};

/*
 * MonetaryValidationGate is the pipeline-facing interface for monetary
 * validation.
 *
 * Separation of concerns:
 *   MonetaryFirewall  — economics logic (arithmetic + authorization rules).
 *   MonetaryValidationGate — runtime interface the pipeline calls before votes.
 *
 * Task 04 will wire this into RuntimeBlockPipeline::produceAndFinalizeNextBlock
 * after BlockStateTransitionValidator and before buildValidatorVotes.
 */
class MonetaryValidationGate {
public:
    static MonetaryValidationGateResult validate(
        const MonetaryPolicy& policy,
        const SupplyDelta& delta,
        const std::vector<MintAuthorization>& authorizations
    );
};

} // namespace nodo::economics

#endif
