#include "core/StateTransitionEngine.hpp"

#include <string>

namespace nodo::core {

StateTransitionPreviewResult StateTransitionEngine::executeBlock(
    const Block& block,
    const StateTransitionPreviewContext& context
) {
    const std::string rejectionReason =
        context.protocolAuthorityRejectionReason();
    if (!rejectionReason.empty()) {
        return StateTransitionPreviewResult::rejected(
            StateTransitionPreviewStatus::INVALID_CONTEXT,
            rejectionReason,
            0
        );
    }

    return StateTransitionPreview::previewBlock(
        block,
        context
    );
}

} // namespace nodo::core
