#include "node/StateArtifactValidator.hpp"

#include "core/StateTransitionEngine.hpp"
#include "node/ProtocolStateTransition.hpp"

#include <exception>
#include <utility>

namespace nodo::node {

ArtifactValidationResult StateArtifactValidator::validate(
    FinalizedArtifactValidationContext& context,
    const FinalizedBlockArtifact& artifact
) {
    const std::string prefix =
        context.rejectionPrefix();

    try {
        core::StateTransitionPreviewContext previewContext =
            ProtocolStateTransition::contextForNextBlock(
                context.runtime(),
                context.minimumFeeRawUnits()
            );

        core::StateTransitionPreviewResult preview =
            core::StateTransitionEngine::executeBlock(
                artifact.block(),
                previewContext
            );

        if (!preview.accepted()) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted block failed canonical protocol replay during reload: " + preview.reason()
            );
        }

        if (preview.stateRoot() != artifact.postStateRoot()) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted block postStateRoot does not match canonical protocol replay."
            );
        }

        if (preview.stateRoot() != artifact.block().stateRoot()) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted block stateRoot does not match the recomputed state transition."
            );
        }

        if (preview.receiptsRoot() != artifact.block().receiptsRoot()) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted block receiptsRoot does not match recomputed receipts."
            );
        }

        if (preview.totalFee() != artifact.totalFee()) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted block totalFeeRawUnits does not match rebuilt transaction fees."
            );
        }

        context.setStatePreview(
            std::move(previewContext),
            std::move(preview)
        );
    } catch (const std::exception& error) {
        return ArtifactValidationResult::rejected(
            prefix + error.what()
        );
    }

    return ArtifactValidationResult::acceptedResult();
}

} // namespace nodo::node
