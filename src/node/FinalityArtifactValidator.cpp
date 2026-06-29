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

        if (!context.runtime().validatorSetHistory().hasSet(block.index())) {
            return ArtifactValidationResult::rejected(
                prefix + "historical validator set is missing for finalized block."
            );
        }
        const core::ValidatorRegistry& historicalValidatorSet =
            context.runtime().validatorSetHistory().setAt(block.index());

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

        const auto& parameters = context.genesisConfig().networkParameters();
        const std::uint64_t historicalTotalVotingWeight =
            historicalValidatorSet.totalConsensusWeight();
        const std::uint64_t historicalRequiredVotingWeight =
            consensus::QuorumCertificateBuilder::requiredVotingWeight(
                historicalTotalVotingWeight,
                parameters.quorumThresholdNumerator(),
                parameters.quorumThresholdDenominator()
            );
        if (artifact.quorumCertificate().requiredVotingWeight() != historicalRequiredVotingWeight ||
            artifact.quorumCertificate().totalVotingWeight() != historicalTotalVotingWeight ||
            artifact.quorumCertificate().validatorSetRoot() != historicalValidatorSet.validatorSetRoot()) {
            return ArtifactValidationResult::rejected(
                prefix + "quorum certificate validator-set weight does not match historical network parameters."
            );
        }

        if (!artifact.quorumCertificate().verify(
                historicalValidatorSet,
                context.cryptoContext().policy(),
                context.cryptoContext().signatureProvider()
            )) {
            return ArtifactValidationResult::rejected(
                prefix + "quorum certificate failed validator vote audit."
            );
        }

        if (!artifact.finalizedRecord().verify(
                historicalValidatorSet,
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
        if (!context.runtime().mutableValidatorSetHistory().recordSet(
                artifact.block().index(), context.runtime().validatorRegistry()
            )) {
            return ArtifactValidationResult::rejected(
                prefix + "historical validator set conflicts at finalized block height."
            );
        }
        const core::ValidatorRegistry& historicalValidatorSet =
            context.runtime().validatorSetHistory().setAt(artifact.block().index());
        const consensus::BlockFinalizationResult finalization =
            consensus::BlockFinalizer::finalizeBlock(
                context.runtime().mutableBlockchain(),
                artifact.block(),
                artifact.quorumCertificate(),
                historicalValidatorSet,
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
