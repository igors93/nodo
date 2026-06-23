#include "core/StateTransitionEngine.hpp"

namespace nodo::core {

StateTransitionPreviewResult StateTransitionEngine::executeBlock(
    const Block& block,
    const StateTransitionPreviewContext& context
) {
    return StateTransitionPreview::previewBlock(
        block,
        context
    );
}

} // namespace nodo::core
