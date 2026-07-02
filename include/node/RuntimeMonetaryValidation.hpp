#ifndef NODO_NODE_RUNTIME_MONETARY_VALIDATION_HPP
#define NODO_NODE_RUNTIME_MONETARY_VALIDATION_HPP

#include "config/NetworkParameters.hpp"
#include "core/Block.hpp"
#include "economics/MonetaryValidationGate.hpp"
#include "economics/SupplyDelta.hpp"
#include "utils/Amount.hpp"

#include <string>

namespace nodo::node {

/*
 * RuntimeMonetaryValidationStatus is the pipeline-level outcome of preparing
 * and running the monetary gate for a candidate block.
 *
 * ACCEPTED                  - gate passed; the candidate may proceed to votes.
 * MONETARY_CONTEXT_UNAVAILABLE - enough information was not available to build a
 *                               real SupplyDelta; the candidate must be rejected.
 * REJECTED_BY_GATE          - the economics::MonetaryValidationGate rejected the
 *                               candidate's supply delta.
 *
 * Security principle:
 * MONETARY_CONTEXT_UNAVAILABLE is always a rejection for vote building.
 * A candidate with unavailable monetary context may not receive votes.
 */
enum class RuntimeMonetaryValidationStatus {
    ACCEPTED,
    MONETARY_CONTEXT_UNAVAILABLE,
    REJECTED_BY_GATE
};

std::string runtimeMonetaryValidationStatusToString(
    RuntimeMonetaryValidationStatus status
);

/*
 * RuntimeMonetaryValidationResult carries the full outcome of pre-vote
 * monetary validation.
 */
class RuntimeMonetaryValidationResult {
public:
    RuntimeMonetaryValidationResult();

    static RuntimeMonetaryValidationResult accepted(
        economics::SupplyDelta supplyDelta,
        economics::MonetaryValidationGateResult gateResult
    );

    static RuntimeMonetaryValidationResult contextUnavailable(std::string reason);

    static RuntimeMonetaryValidationResult rejectedByGate(
        economics::SupplyDelta supplyDelta,
        economics::MonetaryValidationGateResult gateResult
    );

    bool isAccepted() const;
    RuntimeMonetaryValidationStatus status() const;
    const std::string& reason() const;
    const economics::SupplyDelta& supplyDelta() const;
    const economics::MonetaryValidationGateResult& gateResult() const;

    std::string serialize() const;

private:
    bool m_accepted;
    RuntimeMonetaryValidationStatus m_status;
    std::string m_reason;
    economics::SupplyDelta m_supplyDelta;
    economics::MonetaryValidationGateResult m_gateResult;
};

/*
 * RuntimeMonetaryValidation bridges the runtime/pipeline layer to
 * economics::MonetaryValidationGate.
 *
 * It derives a real SupplyDelta for a candidate block using:
 *   - the latest finalized supply as supplyBefore;
 *   - fee burn amounts derived from the transaction fee total;
 *   - voluntary BURN transactions decoded from the candidate block;
 *   - canonical GenesisReward records at epoch settlement boundaries.
 *
 * If feeBurnAmount is zero, a no-op SupplyDelta is built.
 *
 * Security principle:
 * A non-zero feeBurnAmount represents a real supply reduction. The SupplyDelta
 * built here accurately reflects the block's monetary effects for the current
 * pipeline. Returning MONETARY_CONTEXT_UNAVAILABLE is always a rejection; it
 * is never treated as a no-op success.
 */
class RuntimeMonetaryValidation {
public:
    /*
     * validateCandidate builds the SupplyDelta and runs the monetary gate.
     *
     * Parameters:
     *   genesisConfig   - provides genesis supply and chain identity.
     *   candidateBlock  - the block being validated (blockIndex, blockHash, epoch).
     *   feeBurnAmount   - the portion of fees that will be burned (may be zero).
 *
 * Epochs follow ValidatorLifecycle's canonical block-to-epoch mapping. A
 * settlement block uses the settled epoch carried by its ProtectionEpoch.
     */
    /*
     * Full form: uses the provided supplyBefore for cumulative tracking.
     * This is the form called by RuntimeBlockPipeline using
     * runtime.supplyState().latestSupply() as supplyBefore.
     */
    static RuntimeMonetaryValidationResult validateCandidate(
        const config::GenesisConfig& genesisConfig,
        const core::Block& candidateBlock,
        utils::Amount feeBurnAmount,
        utils::Amount supplyBefore
    );

    /*
     * Restricted form: uses genesis supply as supplyBefore.
     * Only valid for block.index() == 1 (the first block after genesis).
     * Rejects any other height to prevent accidental use in multi-block chains.
     * Use the four-argument form for all other heights.
     */
    static RuntimeMonetaryValidationResult validateFirstBlockCandidate(
        const config::GenesisConfig& genesisConfig,
        const core::Block& candidateBlock,
        utils::Amount feeBurnAmount
    );
};

} // namespace nodo::node

#endif
