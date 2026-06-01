#include "node/FinalityArtifactValidator.hpp"

#include "consensus/BlockFinalizer.hpp"

#include <exception>

namespace nodo::node {

ArtifactValidationResult FinalityArtifactValidator::validate(
    FinalizedArtifactValidationContext& context,
    const FinalizedBlockArtifact& artifact
) {
    const std::string prefix =
        context.rejectionPrefix();

    try {
        const core::Block& block =
            artifact.block();

        if (!context.runtime().mutableBlockchain().canAppendBlock(block)) {
            return ArtifactValidationResult::appendRejected(
                "Persisted block cannot append to rebuilt runtime chain."
            );
        }

        if (!consensus::BlockFinalizer::certificateMatchesBlock(
                block,
                artifact.quorumCertificate()
            )) {
            return ArtifactValidationResult::rejected(
                prefix + "quorum certificate does not commit to the persisted block."
            );
        }

        if (!artifact.finalizedRecord().matchesBlock(block)) {
            return ArtifactValidationResult::rejected(
                prefix + "finalized block record does not commit to the persisted block."
            );
        }

        if (artifact.finalizedRecord().quorumCertificate().serialize() !=
            artifact.quorumCertificate().serialize()) {
            return ArtifactValidationResult::rejected(
                prefix + "finalized block record quorum certificate does not match stored certificate."
            );
        }

        if (artifact.quorumCertificate().requiredVoteCount() != context.requiredVoteCount()) {
            return ArtifactValidationResult::rejected(
                prefix + "quorum certificate threshold does not match network parameters."
            );
        }

        if (!artifact.quorumCertificate().verify(
                context.runtime().validatorRegistry(),
                context.cryptoContext().policy(),
                context.cryptoContext().signatureProvider()
            )) {
            return ArtifactValidationResult::rejected(
                prefix + "quorum certificate failed validator vote audit."
            );
        }

        if (!artifact.finalizedRecord().verify(
                context.runtime().validatorRegistry(),
                context.cryptoContext().policy(),
                context.cryptoContext().signatureProvider()
            )) {
            return ArtifactValidationResult::rejected(
                prefix + "finalized block record failed audit."
            );
        }
    } catch (const std::exception& error) {
        return ArtifactValidationResult::rejected(
            prefix + error.what()
        );
    }

    return ArtifactValidationResult::acceptedResult();
}

ArtifactValidationResult FinalityArtifactValidator::applyFinalization(
    FinalizedArtifactValidationContext& context,
    const FinalizedBlockArtifact& artifact
) {
    const std::string prefix =
        context.rejectionPrefix();

    try {
        const consensus::BlockFinalizationResult finalization =
            consensus::BlockFinalizer::finalizeBlock(
                context.runtime().mutableBlockchain(),
                artifact.block(),
                artifact.quorumCertificate(),
                context.runtime().validatorRegistry(),
                context.runtime().mutableFinalizationRegistry(),
                context.cryptoContext().policy(),
                context.cryptoContext().signatureProvider(),
                artifact.finalizedRecord().finalizedAt()
            );

        if (!finalization.finalized() && !finalization.duplicate()) {
            return ArtifactValidationResult::appendRejected(
                prefix + finalization.reason()
            );
        }

        if (finalization.record().serialize() != artifact.finalizedRecord().serialize()) {
            return ArtifactValidationResult::rejected(
                prefix + "stored finalized record does not match reconstructed finalization."
            );
        }
    } catch (const std::exception& error) {
        return ArtifactValidationResult::rejected(
            prefix + error.what()
        );
    }

    return ArtifactValidationResult::acceptedResult();
}

} // namespace nodo::node
