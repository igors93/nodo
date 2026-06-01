#ifndef NODO_NODE_RUNTIME_SUPPLY_STATE_HPP
#define NODO_NODE_RUNTIME_SUPPLY_STATE_HPP

#include "economics/SupplyDelta.hpp"
#include "utils/Amount.hpp"

#include <string>
#include <vector>

namespace nodo::node {

/*
 * RuntimeSupplyState tracks the running monetary supply of the chain as
 * SupplyDelta records are finalized.
 *
 * Security principle:
 * Only supply transitions that passed MonetaryValidationGate may advance the
 * state. The state is never rolled back except on chain reorganization (not
 * implemented yet — Task 06 will handle reload from disk).
 *
 * After finalization of block N, latestSupply() returns supplyDelta(N).supplyAfter().
 * RuntimeMonetaryValidation uses this as supplyBefore for block N+1.
 */
class RuntimeSupplyState {
public:
    RuntimeSupplyState();

    explicit RuntimeSupplyState(utils::Amount genesisSupply);

    utils::Amount latestSupply() const;
    std::uint64_t finalizedDeltaCount() const;
    const std::vector<economics::SupplyDelta>& finalizedDeltas() const;

    /*
     * Advance the supply state with the SupplyDelta from a newly finalized block.
     *
     * The delta must be valid and its supplyBefore must equal latestSupply().
     * Throws std::invalid_argument if the delta is invalid or discontinuous.
     */
    void applyFinalizedDelta(economics::SupplyDelta delta);

    std::string serialize() const;

private:
    utils::Amount m_latestSupply;
    std::vector<economics::SupplyDelta> m_finalizedDeltas;
};

} // namespace nodo::node

#endif
