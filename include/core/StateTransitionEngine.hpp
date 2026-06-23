#ifndef NODO_CORE_STATE_TRANSITION_ENGINE_HPP
#define NODO_CORE_STATE_TRANSITION_ENGINE_HPP

#include "core/Block.hpp"
#include "core/StateTransitionPreview.hpp"
#include "core/StateTransitionPreviewContext.hpp"

namespace nodo::core {

/*
 * StateTransitionEngine is the consensus-facing deterministic execution
 * boundary. It intentionally reuses the battle-tested preview implementation,
 * but exposes a protocol name that means "this result is authoritative for
 * block validation".
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
