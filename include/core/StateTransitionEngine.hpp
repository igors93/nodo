#ifndef NODO_CORE_STATE_TRANSITION_ENGINE_HPP
#define NODO_CORE_STATE_TRANSITION_ENGINE_HPP

#include "core/Block.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"

namespace nodo::core {

/*
 * StateTransitionEngine is the consensus-facing deterministic execution
 * boundary.
 *
 * Unlike StateTransitionPreview, the engine is authoritative: it refuses
 * contexts that cannot fully execute protocol state. A valid engine context
 * must enforce account state, forbid missing accounts, verify chain-bound
 * transaction signatures and carry the canonical protocol-domain executor.
 * Structural previews and account-only rebuild helpers must call
 * StateTransitionPreview directly. Commitment verification and consensus paths
 * must enter this boundary with an authoritative context.
 */
class StateTransitionEngine {
public:
    static StateTransitionPreviewResult executeBlock(
        const Block& block,
        const StateTransitionPreviewContext& context
    );
};

} // namespace nodo::core

#endif
